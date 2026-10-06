#pragma once

#include "../asset_types/image.h"
#include "../render_core/allocator.h"
#include "../render_core/renderResource.h"

struct imageInfo_t;
class AllocatorMemory;
class AliasableImageHeap;
class SwapChain;

enum gpuImageStateFlags_t : uint8_t
{
	GPU_IMAGE_NONE			= 0,
	GPU_IMAGE_READ			= ( 1 << 0 ),
	GPU_IMAGE_WRITE			= ( 1 << 1 ),
	GPU_IMAGE_STORAGE		= ( 1 << 2 ),
	GPU_IMAGE_TRANSFER_SRC	= ( 1 << 3 ),
	GPU_IMAGE_TRANSFER_DST	= ( 1 << 4 ),
	GPU_IMAGE_PERSISTENT	= ( 1 << 5 ),
	GPU_IMAGE_PRESENT		= ( 1 << 6 ),
	GPU_IMAGE_TRANSFER		= ( GPU_IMAGE_TRANSFER_SRC | GPU_IMAGE_TRANSFER_DST ),
	GPU_IMAGE_RW			= ( GPU_IMAGE_READ | GPU_IMAGE_WRITE ),
	GPU_IMAGE_ALL			= 0xFF,
};
DEFINE_ENUM_OPERATORS( gpuImageStateFlags_t, uint8_t )

class GpuImage : public RenderResource
{
protected:
#ifdef USE_VULKAN
	VkImage					vk_image[ MaxFrameStates ];
	VkImageView				vk_view[ MaxFrameStates ];
	Allocation				m_allocation[ MaxFrameStates ];
#endif
	const SwapChain*		m_swapChain = nullptr;		// Only set if the image can be a backbuffer
	gpuImageStateFlags_t	m_flags;					// Sync and usage flags
	swapBuffering_t			m_swapBuffering;			// Single or multi-frame buffering
	imageInfo_t				m_info;						// Image properties
	imageTiling_t			m_tiling;					// Memory layout of the image
	const char*				m_dbgName;					// Debug Name
	int32_t					m_id;						// Bindless resource id
	bool					m_isViewOwned;				// An ImageView owns the memory
	bool					m_ownsAllocation = true;	// An aliased image does not own the backing allocation

	uint32_t GetBufferId( const uint32_t requestedBufferId = 0 ) const;

public:
	GpuImage() = default;

	// Standard images. Both resources and framebuffers
	GpuImage( const char* name, const imageInfo_t& info, const gpuImageStateFlags_t flags, const resourceLifeTime_t lifetime )
	{
		Create( name, info, flags, lifetime );
	}

	// Aliased memory images
	GpuImage( const char* name, const imageInfo_t& info, const gpuImageStateFlags_t flags, const resourceLifeTime_t lifetime, AliasableImageHeap& heap )
	{
		CreateAliased( name, info, flags, lifetime, heap );
	}

	// Interface for swapchain images. These images require special management
	GpuImage( const char* name, const imageInfo_t& info, const gpuImageStateFlags_t flags, const SwapChain* swapChain );

	virtual GpuImage::~GpuImage()
	{
		Destroy();
	}

	int32_t GetId() const
	{
		return m_id;
	}

	void SetId( const int32_t id )
	{
		m_id = id;
	}

	inline uint64_t GetAlignment()
	{
#ifdef USE_VULKAN
		return m_allocation[ 0 ].GetAlignment();
#else
		return 0;
#endif
	}

	inline uint32_t GetBufferCount() const
	{
		return ( m_swapBuffering == swapBuffering_t::MULTI_FRAME ) ? MaxFrameStates : 1;
	}

#ifdef USE_VULKAN
	GpuImage( const GpuImage* gpuImage, const VkImageView views[ MaxFrameStates ] )
	{
		const uint32_t bufferCount = gpuImage->GetBufferCount();
		for ( uint32_t i = 0; i < bufferCount; ++i )
		{
			vk_image[ i ] = gpuImage->vk_image[ i ];
			vk_view[ i ] = views[ i ];
		}
		m_dbgName = gpuImage->GetDebugName();
		m_swapBuffering = gpuImage->m_swapBuffering;
		m_flags = gpuImage->m_flags;
		m_id = gpuImage->m_id;
		m_info = gpuImage->m_info;
		m_isViewOwned = true;
		m_swapChain = gpuImage->m_swapChain;
	}

	// Returns Vulkan resource at a given index. Used for itteration
	inline const VkImage GetVkImageAtIndex( const uint32_t index ) const
	{
		assert( index < MaxFrameStates );
		return vk_image[ index ];
	}

	// Returns Vulkan resource at given index. Used for itteration
	inline const VkImageView GetVkImageViewAtIndex( const uint32_t index ) const
	{
		assert( index < MaxFrameStates );
		return vk_view[ index ];
	}

	// Returns the appropriate buffer for the current frame
	inline VkImage GetVkImage( const uint32_t bufferId ) const
	{
		return vk_image[ GetBufferId( bufferId ) ];
	}

	// Returns the appropriate buffer for the current  frame
	inline VkImageView GetVkImageView( const uint32_t bufferId ) const
	{
		return vk_view[ GetBufferId( bufferId ) ];
	}


	inline void DetachVkImage()
	{
		const uint32_t bufferCount = GetBufferCount();
		for ( uint32_t i = 0; i < bufferCount; ++i )
		{
			vk_image[ i ] = VK_NULL_HANDLE;
		}
	}


	inline void DetachVkImageView()
	{
		const uint32_t bufferCount = GetBufferCount();
		for ( uint32_t i = 0; i < bufferCount; ++i )
		{
			vk_view[ i ] = VK_NULL_HANDLE;
		}
	}
#endif
	inline const char* GetDebugName() const
	{
		return m_dbgName;
	}

	inline gpuImageStateFlags_t GetFlags() const
	{
		return m_flags;
	}

	inline imageInfo_t GetInfo() const
	{
		return m_info;
	}

	inline bool OwnedByImage() const
	{
		return m_isViewOwned;
	}

	void Create( const char* name, const imageInfo_t& info, const gpuImageStateFlags_t flags, const resourceLifeTime_t lifetime );

	void CreateAliased( const char* name, const imageInfo_t& info, const gpuImageStateFlags_t flags, const resourceLifeTime_t lifetime, AliasableImageHeap& heap );

	virtual void Destroy() override;
};
