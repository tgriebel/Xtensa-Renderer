#include "image.h"
#include <syscore/systemUtils.h>
#include <syscore/serializer.h>
#include <syscore/common.h>
#include <gfxcore/io/serializeClasses.h>

#include "../scene/assetBaker.h"
#include "../io/serializeClasses.h"
#include "../io/io.h"
#include "../globals/common.h"
#include "../asset_types/assetLib.h"
#include "../render_resources/gpuImage.h"
#include "../render_resources/aliasableImageHeap.h"


void Image::Create( const imageInfo_t& _info, uint8_t* pixelBytes, const uint32_t byteCount )
{
	{
		m_lifetime = resourceLifeTime_t::ASSET;
		RenderResource::Create( resourceType_t::IMAGE, m_lifetime );
	}

	info = _info;
	info.layers = ( _info.type == IMAGE_TYPE_CUBE ) ? 6 : _info.layers;

	subResourceView.arrayCount = info.layers;
	subResourceView.mipLevels = info.mipLevels;

	generateMips = true;

	assert( cpuImage == nullptr );

	imageBufferInfo_t bufferInfo{};
	bufferInfo.width = _info.width;
	bufferInfo.height = _info.height;
	bufferInfo.layers = _info.layers;
	bufferInfo.mipCount = _info.mipLevels;
	bufferInfo.data = pixelBytes;
	bufferInfo.dataByteCount = byteCount;

	switch ( info.fmt )
	{
		case IMAGE_FMT_R_8:
		{
			cpuImage = new ImageBuffer<uint8_t>( bufferInfo );
		} break;
		case IMAGE_FMT_D_16:
		case IMAGE_FMT_R_16:
		{
			cpuImage = new ImageBuffer<uint16_t>( bufferInfo );
		} break;
		case IMAGE_FMT_D_32:
		case IMAGE_FMT_R_32:
		{
			cpuImage = new ImageBuffer<float>( bufferInfo );
		} break;
		case IMAGE_FMT_RGB_8:
		{
			cpuImage = new ImageBuffer<rgb8_t>( bufferInfo );
		} break;
		case IMAGE_FMT_RGBA_8:
		case IMAGE_FMT_RGBA_8_UNORM:
		{
			cpuImage = new ImageBuffer<rgba8_t>( bufferInfo );
		} break;
		case IMAGE_FMT_RGB_16:
		{
			cpuImage = new ImageBuffer<rgb16_t>( bufferInfo );
		} break;
		case IMAGE_FMT_RGBA_16:
		{
			cpuImage = new ImageBuffer<rgba16_t>( bufferInfo );
		} break;
		default: assert( 0 );
	}
}


void Image::Create( const imageInfo_t& _info, ImageBufferInterface* _cpuImage )
{
	{
		RenderResource::Create( resourceType_t::IMAGE, resourceLifeTime_t::ASSET );
	}

	info = _info;
	info.layers = ( _info.type == IMAGE_TYPE_CUBE ) ? 6 : _info.layers;

	subResourceView.baseArray = 0;
	subResourceView.arrayCount = info.layers;
	subResourceView.baseMip = 0;
	subResourceView.mipLevels = info.mipLevels;

	generateMips = true;

	cpuImage = _cpuImage;
	gpuImage = nullptr;
}

void Image::Create( const imageInfo_t& _info, const char* _name, const gpuImageStateFlags_t _flags, const resourceLifeTime_t _lifetime )
{
	assert( HasFlags( _flags, gpuImageStateFlags_t::GPU_IMAGE_WRITE | gpuImageStateFlags_t::GPU_IMAGE_STORAGE ) );

	m_lifetime = _lifetime;
	m_heap = nullptr;

	{
		RenderResource::Create( resourceType_t::IMAGE, m_lifetime );

		if( !m_resizeFn )
		{
			RegisterResize( FullDimensionResizeFn( _info ) );
		}
	}

	gpuImage = new GpuImage( _name, _info, _flags, m_lifetime );

	info = _info;
	info.layers = ( _info.type == IMAGE_TYPE_CUBE ) ? 6 : _info.layers;

	subResourceView.baseArray = 0;
	subResourceView.arrayCount = info.layers;
	subResourceView.baseMip = 0;
	subResourceView.mipLevels = info.mipLevels;

	generateMips = true;

	cpuImage = nullptr;
}


void Image::CreateAliased( const imageInfo_t& _info, const char* _name, const gpuImageStateFlags_t _flags, const resourceLifeTime_t _lifetime, AliasableImageHeap& heap )
{
	// Aliased images are only supported on framebuffers currently (not iamge storage buffers)
	assert( HasFlags( _flags, gpuImageStateFlags_t::GPU_IMAGE_WRITE ) );

	m_lifetime = _lifetime;
	m_heap = &heap;

	{
		RenderResource::Create( resourceType_t::IMAGE, m_lifetime );

		if ( !m_resizeFn )
		{
			RegisterResize( FullDimensionResizeFn( _info ) );
		}
	}

	gpuImage = new GpuImage( _name, _info, _flags, m_lifetime, heap );

	info = _info;
	info.layers = ( _info.type == IMAGE_TYPE_CUBE ) ? 6 : _info.layers;

	subResourceView.baseArray = 0;
	subResourceView.arrayCount = info.layers;
	subResourceView.baseMip = 0;
	subResourceView.mipLevels = info.mipLevels;

	generateMips = true;

	cpuImage = nullptr;
}


void Image::Destroy()
{
	if ( cpuImage != nullptr )
	{
		delete cpuImage;
		cpuImage = nullptr;
	}

	if( gpuImage != nullptr )
	{
		delete gpuImage;
		gpuImage = nullptr;
	}
}


void Image::DestroyCpuData()
{
	if( cpuImage != nullptr )
	{
		delete cpuImage;
		cpuImage = nullptr;
	}
}


bool Image::OnResize( uint32_t w, uint32_t h )
{
	if( m_resizeFn )
	{
		info = m_resizeFn( w, h );

		if( gpuImage != nullptr )
		{
			const char* name = gpuImage->GetDebugName();
			const gpuImageStateFlags_t flags = gpuImage->GetFlags();
			const resourceLifeTime_t lifeTime = gpuImage->GetLifetime();

			gpuImage->Destroy();

			if ( m_heap != nullptr ) {
				gpuImage->CreateAliased( name, info, flags, lifeTime, *m_heap );
			} else {
				gpuImage->Create( name, info, flags, lifeTime );
			}
		}
		return true;
	}
	return false;
}


Image::ResizeFn Image::FullDimensionResizeFn( const imageInfo_t& info )
{
	return [ info ]( uint32_t w, uint32_t h ) -> imageInfo_t
	{
		imageInfo_t resized = info;
		resized.width = w;
		resized.height = h;

		// If the original had a mip-chain, recalculate the levels (in the default case)
		if( info.mipLevels > 1 ) {
			resized.mipLevels = MipCount( resized.width, resized.height );
		}
		return resized;
	};
}


void Image::Serialize( Serializer* s )
{
	uint32_t version = Version;
	s->Next( version );

	SerializeStruct( s, info );

	if ( s->GetMode() == serializeMode_t::LOAD ) {
		Create( info );
	}

	if ( version == 2 ) {
		s->Next( generateMips );
	}

	cpuImage->Serialize( s );
}


bool ImageLoader::Load( Asset<Image>& imageAsset )
{
	Image& image = imageAsset.Get();

	sourceFile_t imgSource {};
	imgSource.path = m_basePath + m_fileName + "." + m_ext;
	imgSource.name = m_fileName;
	imgSource.isBakedAsset = ( m_ext == "img" ) || ( m_ext == "img.bin" );

	bakedAssetInfo_t info {};
	const bool loadedBaked = LoadBaked( imageAsset, info, imgSource, BakePath + m_basePath, "img.bin" );
	if ( loadedBaked ) {
		return true;
	}

	if( m_ext == "img" ) {
		Serializer s( MB(32), serializeMode_t::LOAD );

		const std::string path = m_basePath + m_fileName + ".img";
		if ( SysCore::FileExists( path ) == false ) {
			return false;
		}
		s.ReadFile( path );

		image.Serialize(&s);

		return ( s.Status() == serializeStatus_t::OK );
	}

	if ( m_cubemap ) {
		return LoadCubeMapImage( ( m_basePath + m_fileName ).c_str(), m_ext.c_str(), image );
	} else {
		if( m_hdr ) {
			return LoadImageHDR( ( m_basePath + m_fileName + "." + m_ext ).c_str(), image );			
		} else {
			return LoadImage( ( m_basePath + m_fileName + "." + m_ext ).c_str(), m_linearColor, image );
		}
	}
}


void ImageLoader::SetBasePath( const std::string& path )
{
	m_basePath = path;
}


void ImageLoader::SetTextureFile( const std::string& file )
{
	SysCore::SplitFileName( file, m_fileName, m_ext );

	if( m_ext == "hdr" ) {
		m_hdr = true;
	}
}


void ImageLoader::LoadAsCubemap( const bool isCubemap )
{
	m_cubemap = isCubemap;
}


void ImageLoader::LoadAsLinear( const bool isLinear )
{
	m_linearColor = isLinear;
}


bool BakedImageLoader::Load( Asset<Image>& imageAsset )
{
	Image& image = imageAsset.Get();

	sourceFile_t imgSource {};
	imgSource.isBakedAsset = true;

	bakedAssetInfo_t info = {};
	const bool loadedBaked = LoadBaked( imageAsset, info, imgSource, m_basePath, m_ext );
	if ( loadedBaked ) {
		return true;
	}
	return false;
}


void BakedImageLoader::SetBasePath( const std::string& path )
{
	m_basePath = path;
}


void BakedImageLoader::SetFileExt( const std::string& ext )
{
	m_ext = ext;
}
