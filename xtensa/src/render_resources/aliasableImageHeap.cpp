#include "aliasableImageHeap.h"

#include "gpuImage.h"
#include "../render_state/deviceContext.h"
#include "../render_state/rhi.h"

#ifdef USE_VULKAN

void AliasableImageHeap::Create( const char* name, const imageInfo_t& refInfo, const gpuImageStateFlags_t flags, const resourceLifeTime_t lifetime )
{
	RenderResource::Create( resourceType_t::MEMORY, lifetime );

	// Build a VkImageCreateInfo matching the reference image so we can query
	// the correct memory requirements (type bits, alignment, size) from the driver.
	VkImageCreateInfo tempImageInfo{};
	tempImageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	tempImageInfo.imageType = VK_IMAGE_TYPE_2D;
	tempImageInfo.extent.width = refInfo.width;
	tempImageInfo.extent.height = refInfo.height;
	tempImageInfo.extent.depth = 1;
	tempImageInfo.mipLevels = refInfo.mipLevels;
	tempImageInfo.arrayLayers = refInfo.layers;
	tempImageInfo.format = vk_GetTextureFormat( refInfo.fmt );
	tempImageInfo.tiling = ( refInfo.tiling == IMAGE_TILING_LINEAR ) ? VK_IMAGE_TILING_LINEAR : VK_IMAGE_TILING_OPTIMAL;
	tempImageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	tempImageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	tempImageInfo.samples = vk_GetSampleCount( refInfo.subsamples );
	tempImageInfo.flags = VK_IMAGE_CREATE_ALIAS_BIT;

	// Build usage flags matching the reference creation flags
	const VkImageAspectFlags aspect = vk_GetColorAspectFlags( refInfo.fmt );
	tempImageInfo.usage = 0;
	if ( ( flags & GPU_IMAGE_WRITE ) != 0 )
	{
		tempImageInfo.usage |= ( aspect & VK_IMAGE_ASPECT_COLOR_BIT ) != 0 ? VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT : 0;
		tempImageInfo.usage |= ( aspect & VK_IMAGE_ASPECT_DEPTH_BIT ) != 0 ? VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT : 0;
		tempImageInfo.usage |= ( aspect & VK_IMAGE_ASPECT_STENCIL_BIT ) != 0 ? VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT : 0;
	}
	tempImageInfo.usage |= ( flags & GPU_IMAGE_STORAGE ) != 0 ? VK_IMAGE_USAGE_STORAGE_BIT : 0;
	tempImageInfo.usage |= ( flags & GPU_IMAGE_READ ) != 0 ? VK_IMAGE_USAGE_SAMPLED_BIT : 0;
	tempImageInfo.usage |= ( flags & GPU_IMAGE_TRANSFER_SRC ) != 0 ? VK_IMAGE_USAGE_TRANSFER_SRC_BIT : 0;
	tempImageInfo.usage |= ( flags & GPU_IMAGE_TRANSFER_DST ) != 0 ? VK_IMAGE_USAGE_TRANSFER_DST_BIT : 0;

	// Create a short-lived image purely to query memory requirements
	VkImage tempImage = VK_NULL_HANDLE;
	VK_CHECK_RESULT( vkCreateImage( context.device, &tempImageInfo, nullptr, &tempImage ) );

	VkMemoryRequirements memReqs{};
	vkGetImageMemoryRequirements( context.device, tempImage, &memReqs );

	vkDestroyImage( context.device, tempImage, nullptr );

	VmaAllocationCreateInfo allocCreateInfo{};
	allocCreateInfo.usage = VMA_MEMORY_USAGE_UNKNOWN;
	allocCreateInfo.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
	allocCreateInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;

	VmaAllocationInfo allocInfo{};
	VK_CHECK_RESULT( vmaAllocateMemory(
		AllocatorMemory::GetVmaAllocator(),
		&memReqs,
		&allocCreateInfo,
		&m_allocation,
		&allocInfo ) );

	m_sizeBytes = allocInfo.size;
	m_resourceByteCount = m_sizeBytes;

	vmaSetAllocationName( AllocatorMemory::GetVmaAllocator(), m_allocation, name );
}


void AliasableImageHeap::Destroy()
{
	if ( m_allocation != VK_NULL_HANDLE )
	{
		vmaFreeMemory( AllocatorMemory::GetVmaAllocator(), m_allocation );
		m_allocation = VK_NULL_HANDLE;
		m_sizeBytes = 0;
	}
}

#endif // USE_VULKAN
