#pragma once

#include "../asset_types/image.h"
#include "../globals/common.h"
#include "../app/window.h"
#include "../render_state/deviceContext.h"
#include "../render_state/rhi.h"
#include "../render_resources/gpuImage.h"
#include "../render_state/frameBuffer.h"

#ifdef USE_VULKAN
QueueFamilyIndices FindQueueFamilies( VkPhysicalDevice device, VkSurfaceKHR surface );
#endif

class SwapChain
{
	static const uint32_t MaxSwapChainBuffers = MaxFrameStates;

private:
	const Window*				m_window;
	uint32_t					m_imageCount;
	imageFmt_t					m_swapChainImageFormat;
	Image						m_swapChainImage;
#ifdef USE_VULKAN
	VkSwapchainKHR				vk_swapChain;
	VkImage						vk_swapChainImages[ MaxSwapChainBuffers ];
	VkImageView					vk_swapChainImageViews[ MaxSwapChainBuffers ];
#endif
	uint32_t					m_imageIndex;

public:

#ifdef USE_VULKAN
	static swapChainInfo_t SwapChain::QuerySwapChainSupport( VkPhysicalDevice device, const VkSurfaceKHR surface );
#endif

	inline imageFmt_t GetBackBufferFormat() const
	{
		return m_swapChainImageFormat;
	}

#ifdef USE_VULKAN
	inline VkSwapchainKHR GetVkObject() const
	{
		return vk_swapChain;
	}

	inline VkImage GetVkImage( const uint32_t bufferId ) const
	{
		return vk_swapChainImages[ bufferId ];
	}

	inline VkImageView GetVkImageView( const uint32_t bufferId ) const
	{
		return vk_swapChainImageViews[ bufferId ];
	}
#endif

	inline uint32_t GetBufferCount() const
	{
		return m_imageCount;
	}


	inline Image* GetBackBuffer()
	{
		return &m_swapChainImage;
	}


	inline const Image* GetBackBuffer() const
	{
		return &m_swapChainImage;
	}


	inline uint32_t GetWidth() const
	{
		return m_swapChainImage.info.width;
	}


	inline uint32_t GetHeight() const
	{
		return m_swapChainImage.info.height;
	}

	void WaitOnFlip( GpuSemaphore& signalSemaphore );

	bool Present( GfxCmdList& context );

	void Create( const Window* _window, const int displayWidth, const int displayHeight );
	void Destroy();
};
