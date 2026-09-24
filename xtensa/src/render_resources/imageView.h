#pragma once
#include "gpuImage.h"
#include "../render_state/deviceContext.h"
#include "../asset_types/image.h"

class ImageView : public Image
{
private:
	const Image* m_sourceImage = nullptr;

public:
	ImageView() : Image()
	{}

	ImageView( const Image& _image ) = delete;

	ImageView( const ImageView* image ) : Image()
	{
		Init( image, image->info, image->m_lifetime );
	}


	ImageView( const Image* image, const imageInfo_t& imageInfo, const resourceLifeTime_t lifetime ) : Image()
	{
		Init( image, imageInfo, lifetime );
	}


	ImageView( const Image* image, const imageInfo_t& imageInfo, const imageSubResourceView_t& subResourceView, const resourceLifeTime_t lifetime ) : Image()
	{
		Init( image, imageInfo, subResourceView, lifetime );
	}

	
	~ImageView()
	{
		Destroy();
	}


	ImageView& operator=( const ImageView* image )
	{
		Init( image, image->info, image->m_lifetime );
		return *this;
	}


	void Init( const Image* image, const imageInfo_t& imageInfo, const resourceLifeTime_t lifetime )
	{
		imageSubResourceView_t subView{};
		subView.baseMip = 0;
		subView.mipLevels = imageInfo.mipLevels;
		subView.baseArray = 0;
		subView.arrayCount = imageInfo.layers;
		subView.aspect = GetColorAspectFlags( info.fmt );
		assert( subView.mipLevels >= 1 );
		assert( subView.arrayCount >= 1 );

		Init( image, imageInfo, subView, lifetime );
	}


	void Init( const Image* image, const imageInfo_t& imageInfo, const imageSubResourceView_t& subView, const resourceLifeTime_t lifetime )
	{
		// Manage Resources
		{
			m_lifetime = lifetime;
			RenderResource::Create( resourceType_t::IMAGE_VIEW, m_lifetime );
		}

		if( image->IsView() ) {
			m_sourceImage = reinterpret_cast<const ImageView*>( image )->m_sourceImage;
		} else {
			m_sourceImage = image;
		}

		info = imageInfo;
		assert( info.layers >= 1 );
		assert( info.mipLevels >= 1 );

		MipDimensions( subView.baseMip, info.width, info.height, &info.width, &info.height );

		subResourceView = subView;
#ifdef USE_VULKAN
		VkImageView views[ MaxFrameStates ];

		const uint32_t bufferCount = m_sourceImage->gpuImage->GetBufferCount();
		for ( uint32_t i = 0; i < bufferCount; ++i )
		{
			views[ i ] = vk_CreateImageView( m_sourceImage->gpuImage->GetVkImage( i ), info, subResourceView, image->gpuImage->GetDebugName(), i );
		}

		gpuImage = new GpuImage( m_sourceImage->gpuImage, views );
#endif
	}


	bool OnResize( const uint32_t w, const uint32_t h ) override
	{
		Destroy();

		info.width = m_sourceImage->info.width;
		info.height = m_sourceImage->info.height;
		info.mipLevels = m_sourceImage->info.mipLevels;

		Init( m_sourceImage, info, subResourceView, m_lifetime );

		return true;
	}


	void Destroy()
	{
		if ( gpuImage != nullptr )
		{
#ifdef USE_VULKAN
			// Views do not own the vulkan image, only the vulkan image view object
			// Detaching prevents the GpuImage destructor from deleting the shared vulkan image
			gpuImage->DetachVkImage();
#endif
			delete gpuImage;
			gpuImage = nullptr;
		}
	}

	virtual bool IsView() const override
	{
		return true;
	}
};
