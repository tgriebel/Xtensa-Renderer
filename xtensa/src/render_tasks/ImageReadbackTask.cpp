#include "ImageReadbackTask.h"

#include <SysCore/serializer.h>
#include <GfxCore/io/serializeClasses.h>

#include "../render_resources/gpuBuffer.h"
#include "../render_binding/bindings.h"

#include "../app/imguiInterface.h"
#include "../globals/assetDefs.h"

extern imguiControls_t g_imguiControls;


std::string ImageReadbackTask::AsString() const
{
	std::stringstream ss;
	ss << "ImageReadbackTask: " << m_name;
	return ss.str();
}


void ImageReadbackTask::Init( const imageReadBackCreateInfo_t& info )
{
	m_context = info.context;
	m_resources = info.resources;
	m_fileName = info.fileName;
	m_name = info.name;
	m_flags = info.flags;
	m_hasWriteback = false;
	m_readbackImage = info.img;

	m_imageArray.SetRenderContext( m_context );

	if ( HasFlags( info.flags, CUBEMAP ) == false )
	{
		m_imageArray.Resize( 1 );
		m_imageArray.BindIndex( 0, m_readbackImage );
	}
	else
	{
		assert( m_readbackImage->info.type == imageType_t::IMAGE_TYPE_CUBE );

		imageInfo_t info = m_readbackImage->info;
		info.type = imageType_t::IMAGE_TYPE_2D;
		m_imageArray.Resize( 6 );

		for ( uint32_t i = 0; i < 6; ++i )
		{
			imageSubResourceView_t subView;
			subView.arrayCount = 1;
			subView.baseArray = vk_MapToGlslCubemapConvention( i );
			subView.baseMip = 0;
			subView.mipLevels = info.mipLevels;
			subView.aspect = GetColorAspectFlags( info.fmt );

			m_cubeViews[ i ].Init( m_readbackImage, info, subView, resourceLifeTime_t::TASK );

			// Sort the slices so serialization is ordered 
			m_imageArray.BindIndex( subView.baseArray, &m_cubeViews[ i ] );
		}
	}

	struct writeBackParms_t
	{
		vec4f		dimensions;
	};

	writeBackParms_t writeBackParms {};

	const uint32_t maxBpp = sizeof( Color ); // Data from the readback is float due to buffer restrictions
	assert( sizeof( vec4f ) == sizeof( Color ) );
	const uint32_t elementsCount = MipPixelCount( m_readbackImage->info.width, m_readbackImage->info.height ) * m_readbackImage->info.layers; // Allocated as for padded out MIPs
	
	m_readbackBuffer.Create(
		"Writeback Buffer",
		swapBuffering_t::MULTI_FRAME,
		resourceLifeTime_t::TASK,
		elementsCount,
		maxBpp,
		bufferType_t::READBACK
	);

	m_resourceBuffer.Create( 
		"Resource buffer",
		swapBuffering_t::SINGLE_FRAME,
		resourceLifeTime_t::TASK,
		1,
		2 * sizeof( writeBackParms ),
		bufferType_t::UNIFORM
	);

	writeBackParms.dimensions = vec4f(	(float)m_readbackImage->info.width,
										(float)m_readbackImage->info.height,
										(float)m_readbackImage->info.layers,
										(float)m_readbackImage->info.mipLevels );

	m_resourceBuffer.SetPos( 0 );
	m_resourceBuffer.CopyData( &writeBackParms, sizeof( writeBackParms ) );

	m_parms = m_context->RegisterBindParm( m_name, m_context->LookupBindSet( bindset_computeImageReadback ) );
}


void ImageReadbackTask::FrameBegin()
{
	m_parms->Bind( BINDING_NAME( globalsBuffer ),		&m_resources->globalConstants );
	m_parms->Bind( BINDING_NAME( computeImageArray ),		&m_imageArray );
	m_parms->Bind( BINDING_NAME( computeParms ),		&m_resourceBuffer );
	m_parms->Bind( BINDING_NAME( computeWrite ),		&m_readbackBuffer );

	GpuTask::OnFrameBegin();
}


void ImageReadbackTask::Execute( CommandList& cmdContext )
{
	if ( HasFlags( m_flags, SCREENSHOT ) ) {
		if( g_imguiControls.captureScreenshot == false ) {
			return;
		}
		g_imguiControls.captureScreenshot = false;
	}

	cmdContext.MarkerBeginRegion( m_name.c_str(), ColorToVector( ColorLGrey ) );
	
	if ( HasFlags( m_flags, TRY_USE_API_COMMAND ) == false )
	{
		const uint32_t blockSize = 8;

		const uint32_t layers = m_readbackImage->info.layers;

		struct pushConstants_t
		{
			vec4f		dimensions;
			uint32_t	imageId;
			int32_t		lod;
			uint32_t	baseOffset;
		};

		uint32_t baseOffset = 0;

		for ( uint32_t mipLevel = 0; mipLevel < m_readbackImage->info.mipLevels; ++mipLevel )
		{
			uint32_t w, h;
			MipDimensions( mipLevel, m_readbackImage->info.width, m_readbackImage->info.height, &w, &h );

			pushConstants_t constants {};
			constants.dimensions = vec4f( (float)w, (float)h, (float)layers, 0.0f );
			constants.imageId = 0;
			constants.lod = mipLevel;
			constants.baseOffset = baseOffset;

			baseOffset += w * h * layers; // Pack tightly without MIP padding
			
			const Asset<GpuProgram>* progAsset = GpuProgramLib().Find( "ImageReadback" );
			cmdContext.Dispatch( *progAsset, *m_parms, &constants, sizeof( pushConstants_t ),  w / blockSize + 1, h / blockSize + 1, layers / blockSize + 1 );
		}
	}
	else
	{
		Transition( &cmdContext, *m_imageArray[ 0 ], GPU_IMAGE_READ, GPU_IMAGE_TRANSFER_SRC );

		VkImageSubresourceLayers subLayers{};
		subLayers.aspectMask = vk_GetColorAspectFlags( m_imageArray[ 0 ]->info.fmt );
		subLayers.baseArrayLayer = 0;
		subLayers.layerCount = 1;
		subLayers.mipLevel = 0;

		VkBufferImageCopy copyParms{};
		copyParms.bufferOffset = 0;
		copyParms.imageExtent.width = m_imageArray[ 0 ]->info.width;
		copyParms.imageExtent.height = m_imageArray[ 0 ]->info.height;
		copyParms.imageExtent.depth = 1;
		copyParms.imageSubresource = subLayers;

		vkCmdCopyImageToBuffer( cmdContext.CommandBuffer(), m_imageArray[ 0 ]->gpuImage->GetVkImage( context.bufferId ), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, m_readbackBuffer.GetVkObject(), 1, &copyParms );

		Transition( &cmdContext, *m_imageArray[ 0 ], GPU_IMAGE_TRANSFER_SRC, GPU_IMAGE_READ );
	}

	cmdContext.MarkerEndRegion();

	m_hasWriteback = true;
}


void ImageReadbackTask::FrameEnd()
{
	if ( m_hasWriteback == false ) {
		return;
	}
	m_hasWriteback = false;

	FlushGPU();

	assert( m_readbackBuffer.VisibleToCpu() );

	m_readbackBuffer.Invalidate();

	// Clear old CPU image
	m_readbackImage->DestroyCpuData();

	imageInfo_t info = m_imageArray[ 0 ]->info;
	info.type = IMAGE_TYPE_2D;
	info.fmt = IMAGE_FMT_RGBA_8;
	info.channels = 4;
	info.layers = 1;

	if ( HasFlags( m_flags, CUBEMAP ) )
	{
		info.type = IMAGE_TYPE_CUBE;
		info.layers = 6;
	}

	/*/
	Color debugLayerColors[ 6 ] =
	{
		ColorRed,
		ColorPink,
		ColorGreen,
		ColorYellow,
		ColorBlue,
		ColorCyan
	};
	*/

	Color* colorData = reinterpret_cast<Color*>( m_readbackBuffer.Get() );

	if ( HasFlags( m_flags, PACKED_HDR ) )
	{
		info.fmt = IMAGE_FMT_RGBA_16;

		m_readbackImage->Create( info );
		
		for ( uint32_t mipLevel = 0; mipLevel < m_readbackImage->info.mipLevels; ++mipLevel )
		{
			for ( uint32_t layer = 0; layer < m_readbackImage->info.layers; ++layer )
			{
				imageRawBuffer_t< rgba16_t > rawBuffer = reinterpret_cast<ImageBuffer<rgba16_t>*>( m_readbackImage->cpuImage )->GetRawBuffer( layer, mipLevel );
				rgba16_t* convertedData = rawBuffer.ptr;

				for ( uint32_t i = 0; i < rawBuffer.pixelCount; ++i )
				{
					convertedData[ i ] = colorData->AsRgba16();
					colorData += 1;
				}
			}
		}
	}
	else
	{
		m_readbackImage->Create( info );

		for ( uint32_t mipLevel = 0; mipLevel < m_readbackImage->info.mipLevels; ++mipLevel )
		{
			for ( uint32_t layer = 0; layer < m_readbackImage->info.layers; ++layer )
			{
				imageRawBuffer_t< rgba8_t > rawBuffer = reinterpret_cast<ImageBuffer<rgba8_t>*>( m_readbackImage->cpuImage )->GetRawBuffer( layer, mipLevel );
				rgba8_t* convertedData = rawBuffer.ptr;

				for ( uint32_t i = 0; i < rawBuffer.pixelCount; ++i )
				{
					Color color = *colorData;

					if( HasFlags( m_flags, SCREENSHOT ) )
					{
						color = LinearToSrgb( color );
						color.Swizzle( RGBA_A, RGBA_B, RGBA_G, RGBA_R );
					}
					convertedData[ i ] = color.AsRgba8();
				
					colorData += 1;
				}
			}
		}
	}

	if ( HasFlags( m_flags, WRITE_TO_DISK ) )
	{
		if ( HasFlags( m_flags, SCREENSHOT ) )
		{
			WriteImage( ( ScreenshotPath + m_fileName ).c_str(), *m_readbackImage );
		}
		else
		{
			bool mipSetting = m_readbackImage->generateMips;
			m_readbackImage->generateMips = false;

			Serializer* s = new Serializer( m_readbackImage->cpuImage->GetByteCount() + 1024, serializeMode_t::STORE );		
			m_readbackImage->Serialize( s );
			s->WriteFile( TexturePath + CodeAssetPath + m_fileName );

			m_readbackImage->generateMips = mipSetting;
			delete s;
		}
	}
}


void ImageReadbackTask::Shutdown()
{
}
