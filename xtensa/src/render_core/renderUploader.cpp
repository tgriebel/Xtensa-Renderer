#include "renderUploader.h"
#include "renderer.h"
#include "resourceContext.h"

#include "../globals/assetDefs.h"

#include "../render_core/renderer.h"
#include "../render_state/rhi.h"
#include "../render_state/cmdContext.h"
#include "../render_resources/gpuAccelerationStructure.h"
#include "../scene/sceneBase.h"
#include "../scene/entity.h"

#define SHADER_STRUCTS_CPP
#include "../../shaders/gpuShared.h"

#include "swapChain.h"
#include "log.h"

extern Scene* g_scene;


void RenderUploader::Boot( RenderContext* context, ResourceContext* resources )
{
	imageFreeSlot = 0;

	renderContext = context;
	resourceContext = resources;

	commands.Create( "Upload Context", renderContext );

	finishedSemaphore.Create( "UploadSemaphore" );
	uploadFence.Create( "Upload Fence" );

#ifdef USE_VULKAN
	finishedSemaphore.waitStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
#endif

	materialBuffer.Reset();

	textureStagingBuffer.Create(
		"Texture Staging",
		swapBuffering_t::SINGLE_FRAME,
		resourceLifeTime_t::REBOOT,
		1,
		MaxTexturingUploadMemory,
		bufferType_t::STAGING
	);

	geometry.vb.Create(
		"VB",
		swapBuffering_t::SINGLE_FRAME,
		resourceLifeTime_t::REBOOT,
		MaxVertices,
		sizeof( vsInput_t ),
		bufferType_t::VERTEX,
#ifdef USE_VULKAN_RTX
		bufferFlags_t::RT_VISIBLE
#else
		bufferFlags_t::NONE
#endif
	);

	geometry.ib.Create(
		"IB",
		swapBuffering_t::SINGLE_FRAME,
		resourceLifeTime_t::REBOOT,
		MaxIndices,
		sizeof( uint32_t ),
		bufferType_t::INDEX,
#ifdef USE_VULKAN_RTX
		bufferFlags_t::RT_VISIBLE
#else
		bufferFlags_t::NONE
#endif
	);

	geometry.stagingBuffer.Create(
		"Geo Staging",
		swapBuffering_t::SINGLE_FRAME,
		resourceLifeTime_t::REBOOT,
		1,
		MaxGeometryUploadMemory,
		bufferType_t::STAGING
	);

	accelerationStructure = new GpuAccelerationStructure();
	accelerationStructure->Create( "Scene AS", resourceLifeTime_t::REBOOT );
}


void RenderUploader::Shutdown()
{
	imageFreeSlot = 0;
	geometry.vbBufElements = 0;
	geometry.ibBufElements = 0;

	commands.Destroy();

	finishedSemaphore.Destroy();
	uploadFence.Destroy();

	delete accelerationStructure;
	accelerationStructure = nullptr;
}


void RenderUploader::OnReboot()
{
	uploadFence.Wait();
	commands.Begin();

	Transition( &commands, *g_swapChain.GetBackBuffer(), swapBuffering_t::MULTI_FRAME, GPU_IMAGE_NONE, GPU_IMAGE_PRESENT );

	commands.End();
	commands.Submit( &uploadFence );
}


void RenderUploader::OnFrameBegin()
{
	uploadFence.Wait();
	commands.Begin();

	RenderResource::TransitionNewImages( &commands );

	UploadModelsToGPU( &commands );

	UploadTextures( &commands );
	UpdateTextureData( &commands );

	commands.End();
	commands.Signal( &finishedSemaphore );
	commands.Submit( &uploadFence );

	UpdateGpuMaterials();
}


void RenderUploader::Upload()
{
	// Assign upload indices for all scene assets
	// FIXME: This should happen per-asset and not scan the list
	{
		const uint32_t materialCount = MaterialLib().Count();
		for( uint32_t i = 0; i < materialCount; ++i )
		{
			Asset<Material>* materialAsset = MaterialLib().Find( i );

			QueueMaterialUpload( *materialAsset );
		}

		const uint32_t textureCount = ImageLib().Count();
		for( uint32_t i = 0; i < textureCount; ++i )
		{
			Asset<Image>* imageAsset = ImageLib().Find( i );
			QueueImageUpload( *imageAsset );
		}

		const uint32_t modelCount = ModelLib().Count();

		for( uint32_t m = 0; m < modelCount; ++m )
		{
			Asset<Model>* modelAsset = ModelLib().Find( m );
			QueueModelUpload( *modelAsset );
		}
	}

	uploadFence.Wait();
	commands.Begin();
	commands.MarkerBeginRegion( "UploadAssets", ColorToVector( Color::Green ) );

	UploadModelsToGPU( &commands );

	UploadTextures( &commands );

	Transition( &commands, *g_swapChain.GetBackBuffer(), swapBuffering_t::MULTI_FRAME, GPU_IMAGE_NONE, GPU_IMAGE_PRESENT );

	commands.MarkerEndRegion();

	commands.End();
	commands.Submit( &uploadFence );

	UpdateGpuMaterials();
}


#ifdef USE_VULKAN
void RenderUploader::CopyGpuBuffer( CommandList* cmdList, GpuBuffer& srcBuffer, GpuBuffer& dstBuffer, VkBufferCopy copyRegion )
{
	VkCommandBuffer commandBuffer = cmdList->CommandBuffer();
	vkCmdCopyBuffer( commandBuffer, srcBuffer.GetVkObject(), dstBuffer.GetVkObject(), 1, &copyRegion );

	dstBuffer.Allocate( copyRegion.size );
}
#endif


void RenderUploader::QueueMaterialUpload( Asset<Material>& materialAsset )
{
	Material& material = materialAsset.Get();

	if( materialAsset.IsLoaded() == false )
	{
		return;
	}
	if( materialAsset.IsUploaded() )
	{
		return;
	}
	uploadMaterials.insert( materialAsset.Handle() );

	for( uint32_t t = 0; t < MaxMaterialTextures; ++t )
	{
		const hdl_t texHandle = material.GetTexture( t );
		if( texHandle.IsValid() )
		{
			Asset<Image>* imageAsset = ImageLib().Find( texHandle );
			QueueImageUpload( *imageAsset );
		}
	}

	materialAsset.QueueUpload();
}


void RenderUploader::QueueModelUpload( Asset<Model>& modelAsset )
{
	if( modelAsset.IsLoaded() == false ) {
		return;
	}

	if( modelAsset.IsUploaded() ) {
		return;
	}

	Model& model = modelAsset.Get();
	if( model.uploadId != -1 ) {
		return;
	}
	model.uploadId = geometry.surfUploads.Count();
	geometry.surfUploads.Grow( model.surfCount );

	modelAsset.QueueUpload();
}


void RenderUploader::QueueImageUpload( Asset<Image>& imageAsset )
{
	if( imageAsset.IsLoaded() == false )
	{
		return;
	}
	if( imageAsset.IsUploaded() )
	{
		return;
	}

	Image* image = &imageAsset.Get();

	if( image == nullptr )
	{
		return;
	}

	if( ( image->gpuImage == nullptr ) || ( image->gpuImage->GetId() < 0 ) )
	{
		uploadImages.insert( imageAsset.Handle() );
	}
	if( imageAsset.IsUploaded() )
	{
		refreshImages.insert( imageAsset.Handle() );
	}
	imageAsset.QueueUpload();
}


void RenderUploader::UpdateTextureData( CommandList* cmdList )
{
	const uint32_t imageCount = static_cast<uint32_t>( refreshImages.size() );
	if( imageCount == 0 )
	{
		return;
	}

	for( auto it = refreshImages.begin(); it != refreshImages.end(); ++it )
	{
		Asset<Image>* imageAsset = ImageLib().Find( *it );
		Image& image = imageAsset->Get();

		Transition( cmdList, image, GPU_IMAGE_NONE, GPU_IMAGE_TRANSFER_DST );

		imageSubResourceView_t subView{};
		subView.baseMip = 0;
		subView.mipLevels = 1;
		subView.baseArray = 0;
		subView.arrayCount = 1;

		UploadImageData( cmdList, image, subView, textureStagingBuffer );

		Transition( cmdList, image, GPU_IMAGE_TRANSFER_DST, GPU_IMAGE_READ );

		imageAsset->CompleteUpload();
	}

	refreshImages.clear();
}


void RenderUploader::UploadTextures( CommandList* cmdList )
{
	textureStagingBuffer.SetPos( 0 );

	const uint32_t textureCount = static_cast<uint32_t>( uploadImages.size() );
	if( textureCount == 0 )
	{
		return;
	}

	// 1. Upload Data
	for( auto it = uploadImages.begin(); it != uploadImages.end(); ++it )
	{
		Asset<Image>* textureAsset = ImageLib().Find( *it );
		if( textureAsset->IsLoaded() == false )
		{
			continue;
		}
		Image& texture = textureAsset->Get();

		// TODO: Allow for recreation for read-only textures
		if( texture.gpuImage != nullptr )
		{
			if( HasFlags( texture.gpuImage->GetFlags(), GPU_IMAGE_WRITE ) )
			{
				continue;
			}
			else
			{
				LogMsg( "Vulkan", logSeverity_t::Warning, "GPU Image already exists!" );
				continue;
			}
		}

		if( texture.cpuImage == nullptr )
		{
			continue;
		}

		gpuImageStateFlags_t flags = ( GPU_IMAGE_READ | GPU_IMAGE_TRANSFER_SRC | GPU_IMAGE_TRANSFER_DST );

		texture.gpuImage =
			new GpuImage( textureAsset->GetName().c_str(), texture.info, flags, resourceLifeTime_t::REBOOT );

		Transition( cmdList, texture, GPU_IMAGE_NONE, GPU_IMAGE_TRANSFER_DST );

		imageSubResourceView_t subView{};
		subView.baseMip = 0;
		subView.mipLevels = texture.generateMips ? 1 : texture.info.mipLevels;
		subView.baseArray = 0;
		subView.arrayCount = texture.info.layers;

		UploadImageData( cmdList, texture, subView, textureStagingBuffer );

		assert( imageFreeSlot < MaxImageDescriptors );
		texture.gpuImage->SetId( imageFreeSlot );
		++imageFreeSlot;
	}

	// 2. Generate MIPS
	for( auto it = uploadImages.begin(); it != uploadImages.end(); ++it )
	{
		Asset<Image>* textureAsset = ImageLib().Find( *it );
		if( textureAsset->IsLoaded() == false )
		{
			continue;
		}
		Image& texture = textureAsset->Get();

		if( texture.gpuImage == nullptr )
		{
			continue;
		}
		if( HasFlags( texture.gpuImage->GetFlags(), GPU_IMAGE_WRITE ) )
		{
			continue;
		}
		if( texture.generateMips == false )
		{
			Transition( cmdList, texture, GPU_IMAGE_TRANSFER_DST, GPU_IMAGE_READ );
			continue;
		}
		GenerateMipmaps( cmdList, texture );

		textureAsset->CompleteUpload();
	}

	// 3. Add to resource type lists
	{
		// Find first cubemap. FIXME: Hacky, just done so there aren't nulls in the list
		Image* firstCube = nullptr;
		for( auto it = uploadImages.begin(); it != uploadImages.end(); ++it )
		{
			Asset<Image>* textureAsset = ImageLib().Find( *it );
			if( textureAsset->IsLoaded() == false )
			{
				continue;
			}
			if( textureAsset->Get().info.type == IMAGE_TYPE_CUBE )
			{
				firstCube = &textureAsset->Get();
				break;
			}
		}
		assert( firstCube != nullptr );

		// Fill assigned slots
		for( uint32_t i = 0; i < imageFreeSlot; ++i )
		{
			Asset<Image>* textureAsset = ImageLib().Find( i );
			if( textureAsset->IsLoaded() == false )
			{
				continue;
			}

			Image& texture = textureAsset->Get();

			if( texture.gpuImage == nullptr )
			{
				continue;
			}

			const int uploadId = texture.gpuImage->GetId();

			switch( texture.info.type )
			{
			case IMAGE_TYPE_2D:
				resourceContext->gpuImages2D.BindIndex( uploadId, &texture );
				resourceContext->gpuImagesCube.BindIndex( uploadId, firstCube );
				break;
			case IMAGE_TYPE_CUBE:
				resourceContext->gpuImages2D.BindIndex( uploadId, &ImageLib().GetDefault()->Get() );
				resourceContext->gpuImagesCube.BindIndex( uploadId, &texture );
				break;
			}
		}

		// Fill defaults
		for( uint32_t i = imageFreeSlot; i < MaxImageDescriptors; ++i )
		{
			resourceContext->gpuImages2D.BindIndex( i, &ImageLib().GetDefault()->Get() );
			resourceContext->gpuImagesCube.BindIndex( i, firstCube );
		}
	}

	uploadImages.clear();
}


void RenderUploader::UpdateGpuMaterials()
{
	for( auto it = uploadMaterials.begin(); it != uploadMaterials.end(); ++it )
	{
		Asset<Material>* matAsset = MaterialLib().Find( *it );
		Material& m = matAsset->Get();
		if( m.uploadId < 0 )
		{
			m.uploadId = materialBuffer.Count();
			materialBuffer.Append( gpuMaterial_t() );
		}
		matAsset->CompleteUpload();

		assert( m.uploadId < MaxMaterials );

		gpuMaterial_t& materialObject = materialBuffer[ m.uploadId ];

		// TODO: move texture assignment, should use general helper function
		if( m.usage == MATERIAL_USAGE_CODE )
		{
			for( uint32_t t = 0; t < MaxMaterialTextures; ++t )
			{
				materialObject.textureId[ t ] = (int)m.GetTexture( t ).Get();
			}
		}
		else
		{
			for( uint32_t t = 0; t < MaxMaterialTextures; ++t )
			{
				const hdl_t handle = m.GetTexture( t );
				if( handle.IsValid() )
				{
					const hdl_t imageId = m.GetTexture( t );
					const Asset<Image>* imageAsset = ImageLib().Find( imageId );
					const Image& image = imageAsset->Get();
					assert( image.gpuImage != nullptr );

					const int uploadId = image.gpuImage->GetId();
					assert( uploadId >= 0 );
					materialObject.textureId[ t ] = uploadId;
				}
				else
				{
					materialObject.textureId[ t ] = -1;
				}
			}
		}

		m.TranslateToGpuMaterial( materialObject );
	}

	resourceContext->materialBuffers.SetPos( 0 );
	resourceContext->materialBuffers.CopyData( materialBuffer.Ptr(), sizeof( gpuMaterial_t ) * materialBuffer.Count() );

	uploadMaterials.clear();
}


void RenderUploader::UploadModelsToGPU( CommandList* cmdList )
{
	geometry.stagingBuffer.SetPos( 0 );

	const uint32_t modelCount = ModelLib().Count();

	for( uint32_t m = 0; m < modelCount; ++m )
	{
		Asset<Model>* modelAsset = ModelLib().Find( m );
		Model& model = modelAsset->Get();
		if( modelAsset->IsUploaded() )
		{
			continue;
		}
		if( model.uploadId == -1 )
		{
			continue;
		}

		for( uint32_t s = 0; s < model.surfCount; ++s )
		{
			Surface& surf = model.surfs[ s ];
			surfaceUpload_t& upload = geometry.surfUploads[ model.uploadId + s ];

			upload.vertexOffset = geometry.vbBufElements;
			upload.firstIndex = geometry.ibBufElements;

			// Upload Vertex Buffer
			{
				// Create vertex stream data
				std::vector<vsInput_t> vertexStream;
				const uint32_t vertexCount = static_cast<uint32_t>( surf.vertices.size() );
				vertexStream.resize( vertexCount );
				for( uint32_t vIx = 0; vIx < vertexCount; ++vIx )
				{
					vertexStream[ vIx ].inPosition = surf.vertices[ vIx ].pos;
					vertexStream[ vIx ].inColor = ColorToVector( surf.vertices[ vIx ].color );
					vertexStream[ vIx ].inNormal = surf.vertices[ vIx ].normal;
					vertexStream[ vIx ].inTangent = surf.vertices[ vIx ].tangent;
					vertexStream[ vIx ].inBitangent = surf.vertices[ vIx ].bitangent;
					vertexStream[ vIx ].uv0.x = surf.vertices[ vIx ].uv0.x;
					vertexStream[ vIx ].uv0.y = surf.vertices[ vIx ].uv0.y;
					vertexStream[ vIx ].uv1.x = surf.vertices[ vIx ].uv1.x;
					vertexStream[ vIx ].uv1.y = surf.vertices[ vIx ].uv1.y;
				}

				// Copy stream to staging buffer
				VkDeviceSize vbCopySize = sizeof( vertexStream[ 0 ] ) * vertexCount;

				VkBufferCopy vbCopyRegion{ };
				vbCopyRegion.size = vbCopySize;
				vbCopyRegion.srcOffset = geometry.stagingBuffer.GetSize();
				vbCopyRegion.dstOffset = geometry.vb.GetSize();

				geometry.stagingBuffer.CopyData( vertexStream.data(), static_cast<size_t>( vbCopySize ) );

				CopyGpuBuffer( cmdList, geometry.stagingBuffer, geometry.vb, vbCopyRegion );

				upload.vertexCount = vertexCount;
				geometry.vbBufElements += vertexCount;

				assert( geometry.vbBufElements < MaxVertices );
			}

			// Upload Index Buffer
			{
				// IB Copy
				VkDeviceSize ibCopySize = sizeof( surf.indices[ 0 ] ) * surf.indices.size();

				VkBufferCopy ibCopyRegion{ };
				ibCopyRegion.size = ibCopySize;
				ibCopyRegion.srcOffset = geometry.stagingBuffer.GetSize();
				ibCopyRegion.dstOffset = geometry.ib.GetSize();
				CopyGpuBuffer( cmdList, geometry.stagingBuffer, geometry.ib, ibCopyRegion );

				geometry.stagingBuffer.CopyData( surf.indices.data(), static_cast<size_t>( ibCopySize ) );

				const uint32_t indexCount = static_cast<uint32_t>( surf.indices.size() );
				upload.indexCount = indexCount;
				geometry.ibBufElements += indexCount;

				assert( geometry.ibBufElements < MaxIndices );
			}

#ifdef USE_VULKAN_RTX
			if( renderContext->config.rayTracingEnabled )
			{
				rtSurfaceInfo_t surfInfo{};
				surfInfo.geometry = &geometry;
				surfInfo.surface = &upload;
				surfInfo.name = modelAsset->GetName().c_str();

				accelerationStructure->AddGeometry( cmdList, surfInfo );
			}
#endif
		}

		modelAsset->CompleteUpload();
	}

#ifdef USE_VULKAN_RTX
	if( renderContext->config.rayTracingEnabled )
	{
		accelerationStructure->BuildPendingGeometry( cmdList );
		accelerationStructure->Update( cmdList );
	}
#endif
}