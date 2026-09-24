#pragma once

#include "../globals/common.h"

class GpuSemaphore
{
#ifdef USE_VULKAN
private:
	VkSemaphore				semaphores[ MaxFrameStates ];

public:
	VkPipelineStageFlagBits	waitStage;
#endif
public:
	void					Create( const char* name );
	void					Destroy();
#ifdef USE_VULKAN
	VkSemaphore&			VkObject();
	VkSemaphore				GetVkObject() const;
#endif
};


class GpuTimelineSemaphore
{
#ifdef USE_VULKAN
private:
	VkSemaphore				semaphore;

public:
	VkPipelineStageFlagBits	waitStage;
#endif
public:
	void					Create( const char* name );
	void					Destroy();
#ifdef USE_VULKAN
	VkSemaphore&			VkObject();
	VkSemaphore				GetVkObject() const;
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