#include "gpuImage.h"

#include "../render_state/deviceContext.h"
#include "../render_state/rhi.h"
#include "../asset_types/image.h"
#include "../render_core/swapChain.h"
#include "../render_resources/aliasableImageHeap.h"

// TODO: move
#ifdef USE_VULKAN	
static VkImageCreateInfo vk_GetImageCreateInfo( const imageInfo_t& info, const gpuImageStateFlags_t flags )
{
	VkImageCreateInfo imageInfo{ };
	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.imageType = VK_IMAGE_TYPE_2D;
	imageInfo.extent.width = static_cast<uint32_t>( info.width );
	imageInfo.extent.height = static_cast<uint32_t>( info.height );
	imageInfo.extent.depth = 1;
	imageInfo.mipLevels = info.mipLevels;
	imageInfo.arrayLayers = ( info.type == IMAGE_TYPE_CUBE ) ? 6 : info.layers;
	imageInfo.format = vk_GetTextureFormat( info.fmt );
	imageInfo.tiling = ( info.tiling == IMAGE_TILING_LINEAR ) ? VK_IMAGE_TILING_LINEAR : VK_IMAGE_TILING_OPTIMAL;
	imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imageInfo.samples = vk_GetSampleCount( info.subsamples );

	// Aspect inferred from format, masked by info.aspect for image views
	const VkImageAspectFlags aspect = vk_GetColorAspectFlags( info.fmt );
	
	imageInfo.usage = 0;
	if( ( flags & GPU_IMAGE_WRITE ) != 0 )
	{
		imageInfo.usage |= ( aspect & VK_IMAGE_ASPECT_COLOR_BIT ) != 0 ? VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT : 0;
		imageInfo.usage |= ( aspect & VK_IMAGE_ASPECT_DEPTH_BIT ) != 0 ? VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT : 0;
		imageInfo.usage |= ( aspect & VK_IMAGE_ASPECT_STENCIL_BIT ) != 0 ? VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT : 0;
	}
	if( ( flags & GPU_IMAGE_STORAGE ) != 0 )
	{
		imageInfo.usage |= VK_IMAGE_USAGE_STORAGE_BIT;
	}
	imageInfo.usage |= ( flags & GPU_IMAGE_READ ) != 0 ? VK_IMAGE_USAGE_SAMPLED_BIT : 0;
	imageInfo.usage |= ( flags & GPU_IMAGE_TRANSFER_SRC ) != 0 ? VK_IMAGE_USAGE_TRANSFER_SRC_BIT : 0;
	imageInfo.usage |= ( flags & GPU_IMAGE_TRANSFER_DST ) != 0 ? VK_IMAGE_USAGE_TRANSFER_DST_BIT : 0;

	imageInfo.flags = 0;
	imageInfo.flags |= ( info.type == IMAGE_TYPE_CUBE ) ? VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT : 0;

	return imageInfo;
}
#endif


GpuImage::GpuImage( const char* name, const imageInfo_t& info, const gpuImageStateFlags_t flags, const SwapChain* swapChain )
{
	RenderResource::Create( resourceType_t::SWAPCHAIN, resourceLifeTime_t::UNMANAGED );

	m_dbgName = name;
	m_swapBuffering = ( flags & GPU_IMAGE_PERSISTENT ) != 0 ? swapBuffering_t::MULTI_FRAME : swapBuffering_t::SINGLE_FRAME;

	const uint32_t bufferCount = GetBufferCount();
	for( uint32_t i = 0; i < bufferCount; ++i )
	{
#ifdef USE_VULKAN
		vk_image[ i ] = swapChain->GetVkImage( i );
		vk_view[ i ] = swapChain->GetVkImageView( i );
#endif
	}

	m_id = -1;
	m_flags = flags;
	m_info = info;
	m_isViewOwned = true;
}


void GpuImage::Create( const char* name, const imageInfo_t& info, const gpuImageStateFlags_t flags, const resourceLifeTime_t lifetime )
{
	// Managed Resource
	{
		RenderResource::Create( resourceType_t::GPU_IMAGE, lifetime );
	}
	assert( name != nullptr && name[0] != '\0' );
	m_isViewOwned = false;
	m_info = info;
	m_flags = flags;
	m_dbgName = name;
	m_swapBuffering = ( flags & GPU_IMAGE_PERSISTENT ) != 0 ? swapBuffering_t::MULTI_FRAME : swapBuffering_t::SINGLE_FRAME;

#ifdef USE_VULKAN
	{
		VkImageCreateInfo imageInfo = vk_GetImageCreateInfo( info, flags );

		const VkImageAspectFlags aspect = vk_GetColorAspectFlags( info.fmt );

		VkImageStencilUsageCreateInfo stencilUsage{};
		if ( IsDepthStencilCompatible( info.fmt ) )
		{
			stencilUsage.sType = VK_STRUCTURE_TYPE_IMAGE_STENCIL_USAGE_CREATE_INFO;
			stencilUsage.stencilUsage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
			imageInfo.flags |= VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT;
			imageInfo.pNext = &stencilUsage;
		}

		VmaAllocationCreateInfo allocCreateInfo = {};
		allocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;

		//m_resourceMemoryRegion = ?;

		const uint32_t bufferCount = GetBufferCount();
		for ( uint32_t i = 0; i < bufferCount; ++i )
		{
			VmaAllocation allocation;
			VmaAllocationInfo allocInfo;

			VkResult result = vmaCreateImage(
				AllocatorMemory::GetVmaAllocator(),
				&imageInfo, &allocCreateInfo,
				&vk_image[ i ],
				&allocation, &allocInfo );

			if ( result != VK_SUCCESS ) {
				THROW_ERROR( "Image '" + std::string( name ) + "' [" + std::to_string( i ) + "] could not allocate "
					+ std::to_string( imageInfo.extent.width ) + "x" + std::to_string( imageInfo.extent.height )
					+ " (format: " + std::to_string( imageInfo.format ) + ")" );
			}

			vmaSetAllocationName( AllocatorMemory::GetVmaAllocator(), allocation, name );

			m_allocation[ i ].m_allocation = allocation;
			m_allocation[ i ].m_info = allocInfo;

			m_resourceByteCount = allocInfo.size;

			if ( m_dbgName != "" )
			{
				vk_SetObjectName( (uint64_t)vk_image[ i ], VK_OBJECT_TYPE_IMAGE, vk_BuildObjectName( "Image", m_dbgName, i ).c_str() );
			}

			vk_view[ i ] = vk_CreateImageView( vk_image[ i ], info, m_dbgName, i );
		}
	}
#endif
}


void GpuImage::Destroy()
{
#ifdef USE_VULKAN
	assert( context.device != VK_NULL_HANDLE );
	if( context.device == VK_NULL_HANDLE ) {
		return;
	}

	const uint32_t bufferCount = GetBufferCount();
	for ( uint32_t i = 0; i < bufferCount; ++i )
	{
		if( vk_view[ i ] != VK_NULL_HANDLE )
		{
			vkDestroyImageView( context.device, vk_view[ i ], nullptr );
			vk_view[ i ] = VK_NULL_HANDLE;
		}

		if ( vk_image[ i ] != VK_NULL_HANDLE )
		{
			if ( m_ownsAllocation )
			{
				// Standard images. VMA owns both the image and backing allocation
				vmaDestroyImage( AllocatorMemory::GetVmaAllocator(), vk_image[ i ], m_allocation[ i ].m_allocation );
			}
			else
			{
				// Aliased images. Backing memory allocation is owned by aliased heap
				vkDestroyImage( context.device, vk_image[ i ], nullptr );
			}
			vk_image[ i ] = VK_NULL_HANDLE;
			m_allocation[ i ].m_allocation = VK_NULL_HANDLE;
			m_allocation[ i ].m_info = {};
		}
	}

	m_ownsAllocation = true;
#endif
}


void GpuImage::CreateAliased( const char* name, const imageInfo_t& info, const gpuImageStateFlags_t flags, const resourceLifeTime_t lifetime, AliasableImageHeap& heap )
{
	assert( heap.GetLifetime() == resourceLifeTime_t::REBOOT );

#ifdef USE_VULKAN
	RenderResource::Create( resourceType_t::GPU_IMAGE, lifetime );

	assert( name != nullptr && name[ 0 ] != '\0' );
	m_isViewOwned = false;
	m_ownsAllocation = false;
	m_id = -1;
	m_info = info;
	m_flags = flags;
	m_dbgName = name;
	// Aliased images share a single heap allocation — multi-buffering would alias
	// the same memory across frames simultaneously, violating non-simultaneous-use.
	assert( ( flags & GPU_IMAGE_PERSISTENT ) == 0 );
	m_swapBuffering = swapBuffering_t::SINGLE_FRAME;

	VkImageCreateInfo imageInfo = vk_GetImageCreateInfo( info, flags );

	imageInfo.flags |= VK_IMAGE_CREATE_ALIAS_BIT;

	VkResult result = vmaCreateAliasingImage(
		AllocatorMemory::GetVmaAllocator(),
		heap.GetAllocation(),
		&imageInfo,
		&vk_image[ 0 ] );

	if ( result != VK_SUCCESS )
	{
		THROW_ERROR( std::string( "AliasableImage '" ) + name + "' failed — "
			+ "heap may be too small for " + std::to_string( info.width ) + "x" + std::to_string( info.height )
			+ " (format " + std::to_string( static_cast<int>( info.fmt ) ) + ")" );
	}

	m_allocation[ 0 ].m_allocation = VK_NULL_HANDLE; // Not owned
	m_allocation[ 0 ].m_info       = {};

	VkMemoryRequirements memRequirements;
	vkGetImageMemoryRequirements( context.device, vk_image[ 0 ], &memRequirements );
	m_allocation[ 0 ].m_alignment = memRequirements.alignment;

	m_resourceByteCount = memRequirements.size;

	if ( m_dbgName != nullptr && m_dbgName[ 0 ] != '\0' ) {
		vk_SetObjectName( (uint64_t)vk_image[ 0 ], VK_OBJECT_TYPE_IMAGE, vk_BuildObjectName( "AliasImage", m_dbgName, 0 ).c_str() );
	}

	vk_view[ 0 ] = vk_CreateImageView( vk_image[ 0 ], info, m_dbgName, 0 );
#endif
}