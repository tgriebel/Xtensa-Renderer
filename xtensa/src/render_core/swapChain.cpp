#include "swapChain.h"


#ifdef USE_VULKAN
VkPresentModeKHR vk_ChooseSwapPresentMode( const std::vector<VkPresentModeKHR>& availablePresentModes )
{
	for ( const auto& availablePresentMode : availablePresentModes )
	{
		if ( availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR )
		{
			return availablePresentMode;
		}
	}

	return VK_PRESENT_MODE_FIFO_KHR;
}


static VkSurfaceFormatKHR vk_ChooseSwapSurfaceFormat( const std::vector<VkSurfaceFormatKHR>& availableFormats )
{
	for ( const auto& availableFormat : availableFormats ) {
		if ( availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR ) {
			return availableFormat;
		}
	}
	return availableFormats[ 0 ];
}


static VkExtent2D vk_ChooseSwapExtent( const VkSurfaceCapabilitiesKHR& capabilities, const int displayWidth, const int displayHeight )
{
	if ( capabilities.currentExtent.width != UINT32_MAX ) {
		return capabilities.currentExtent;
	}
	else
	{
		VkExtent2D actualExtent = {
			static_cast<uint32_t>( displayWidth ),
			static_cast<uint32_t>( displayHeight )
		};

		actualExtent.width = std::max( capabilities.minImageExtent.width, std::min( capabilities.maxImageExtent.width, actualExtent.width ) );
		actualExtent.height = std::max( capabilities.minImageExtent.height, std::min( capabilities.maxImageExtent.height, actualExtent.height ) );

		return actualExtent;
	}
}
#endif


swapChainInfo_t SwapChain::QuerySwapChainSupport( VkPhysicalDevice device, const VkSurfaceKHR surface )
{
	swapChainInfo_t info;
#ifdef USE_VULKAN
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR( device, surface, &info.capabilities );

	uint32_t formatCount;
	vkGetPhysicalDeviceSurfaceFormatsKHR( device, surface, &formatCount, nullptr );

	if ( formatCount != 0 )
	{
		info.formats.resize( formatCount );
		vkGetPhysicalDeviceSurfaceFormatsKHR( device, surface, &formatCount, info.formats.data() );
	}

	uint32_t presentModeCount;
	vkGetPhysicalDeviceSurfacePresentModesKHR( device, surface, &presentModeCount, nullptr );

	if ( presentModeCount != 0 )
	{
		info.presentModes.resize( presentModeCount );
		vkGetPhysicalDeviceSurfacePresentModesKHR( device, surface, &presentModeCount, info.presentModes.data() );
	}
#endif
	return info;
}


void SwapChain::WaitOnFlip( GpuSemaphore& signalSemaphore )
{
#ifdef USE_VULKAN
	VK_CHECK_RESULT( vkAcquireNextImageKHR( context.device, GetVkObject(), UINT64_MAX, signalSemaphore.GetVkObject(), VK_NULL_HANDLE, &m_imageIndex ) );
#endif
}


bool SwapChain::Present( GfxCmdList& gfxContext )
{
	bool ret = 0;

#ifdef USE_VULKAN
	VkSubmitInfo submitInfo{ };
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

	VkSemaphore waitSemaphores[] = { gfxContext.renderFinishedSemaphore.GetVkObject() };

	VkPresentInfoKHR presentInfo{ };
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = waitSemaphores;

	VkSwapchainKHR swapChains[] = { GetVkObject() };
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = swapChains;
	presentInfo.pImageIndices = &m_imageIndex;
	presentInfo.pResults = nullptr; // Optional

	VkResult result = vkQueuePresentKHR( context.presentQueue, &presentInfo );

	ret = !( ( result == VK_ERROR_OUT_OF_DATE_KHR ) || ( result == VK_SUBOPTIMAL_KHR ) );
#endif
	return ret;
}	


void SwapChain::Create( const Window* _window, const int displayWidth, const int displayHeight )
{
	m_window = _window;
	swapChainInfo_t swapChainSupport = QuerySwapChainSupport( context.physicalDevice, m_window->vk_surface );

#ifdef USE_VULKAN
	VkSurfaceFormatKHR surfaceFormat = vk_ChooseSwapSurfaceFormat( swapChainSupport.formats );
	VkPresentModeKHR presentMode = vk_ChooseSwapPresentMode( swapChainSupport.presentModes );
	VkExtent2D extent = vk_ChooseSwapExtent( swapChainSupport.capabilities, displayWidth, displayHeight );

	m_imageCount = swapChainSupport.capabilities.minImageCount + 1;
	if ( swapChainSupport.capabilities.maxImageCount > 0 && m_imageCount > swapChainSupport.capabilities.maxImageCount ) {
		m_imageCount = swapChainSupport.capabilities.maxImageCount;
	}
	m_imageIndex = 0;

	VkSwapchainCreateInfoKHR createInfo{ };
	createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	createInfo.surface = m_window->vk_surface;
	createInfo.oldSwapchain = vk_swapChain;
	createInfo.minImageCount = m_imageCount;
	createInfo.imageFormat = surfaceFormat.format;
	createInfo.imageColorSpace = surfaceFormat.colorSpace;
	createInfo.imageExtent = extent;
	createInfo.imageArrayLayers = 1;
	createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

	if ( context.queueFamilyIndices[ QUEUE_GRAPHICS ] != context.queueFamilyIndices[ QUEUE_PRESENT ] )
	{
		createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
		createInfo.queueFamilyIndexCount = QUEUE_COUNT;
		createInfo.pQueueFamilyIndices = context.queueFamilyIndices;
	}
	else
	{
		createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		createInfo.queueFamilyIndexCount = 0;
		createInfo.pQueueFamilyIndices = nullptr;
	}

	createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
	createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	createInfo.presentMode = presentMode;
	createInfo.clipped = VK_TRUE;
	createInfo.oldSwapchain = VK_NULL_HANDLE;

	VK_CHECK_RESULT( vkCreateSwapchainKHR( context.device, &createInfo, nullptr, &vk_swapChain ) );

	vkGetSwapchainImagesKHR( context.device, vk_swapChain, &m_imageCount, nullptr );
	assert( m_imageCount <= MaxSwapChainBuffers );
	vkGetSwapchainImagesKHR( context.device, vk_swapChain, &m_imageCount, vk_swapChainImages );

	m_swapChainImageFormat = vk_GetTextureFormat( surfaceFormat.format );

	imageInfo_t& info = m_swapChainImage.info;

	info.fmt = m_swapChainImageFormat;
	info.width = extent.width;
	info.height = extent.height;
	info.layers = 1;
	info.mipLevels = 1;
	info.channels = 4;
	info.subsamples = IMAGE_SMP_1;
	info.tiling = IMAGE_TILING_LINEAR;
	info.type = IMAGE_TYPE_2D;

	m_swapChainImage.subResourceView.baseArray = 0;
	m_swapChainImage.subResourceView.arrayCount = 1;	
	m_swapChainImage.subResourceView.baseMip = 0;
	m_swapChainImage.subResourceView.mipLevels = 1;
	m_swapChainImage.subResourceView.aspect = IMAGE_ASPECT_ALL;

	for ( uint32_t i = 0; i < m_imageCount; ++i )
	{
		vk_swapChainImageViews[ i ] = vk_CreateImageView( vk_swapChainImages[ i ], info, "Swapchain", i );
	}

	gpuImageStateFlags_t flags = GPU_IMAGE_PRESENT | GPU_IMAGE_PERSISTENT;
	m_swapChainImage.gpuImage = new GpuImage( "_backbuffer", info, flags, this );
#endif
}


void SwapChain::Destroy()
{
	// Vulkan swapchain images are a bit special since they need to be destroyed with the swapchain
	m_swapChainImage.gpuImage->DetachVkImage();
	delete m_swapChainImage.gpuImage;

#ifdef USE_VULKAN
	vkDestroySwapchainKHR( context.device, vk_swapChain, nullptr );
#endif
}