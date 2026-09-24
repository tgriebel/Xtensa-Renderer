#pragma once

#include "../globals/common.h"

class GpuSemaphore
{
#ifdef USE_VULKAN
private:
	VkSemaphore				semaphores[ MaxFrameStates ];
	bool					isTimeline = false;

public:
	VkPipelineStageFlagBits	waitStage;
#endif
public:
	void					Create( const char* name, const bool isBinary = true );
	void					Destroy();
#ifdef USE_VULKAN
	VkSemaphore&			VkObject();
	VkSemaphore				GetVkObject() const;
	bool					IsTimeline() const { return isTimeline; }
#endif
};


class GpuFence
{
private:
#ifdef USE_VULKAN
	VkFence	fence;
#endif
public:
	void			Create( const char* name );
	void			Destroy();
	void			Wait();
#ifdef USE_VULKAN
	VkFence&		VkObject();
	VkFence			GetVkObject() const;
#endif
};