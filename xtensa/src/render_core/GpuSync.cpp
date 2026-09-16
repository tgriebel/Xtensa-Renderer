#include "GpuSync.h"
#include "../render_state/deviceContext.h"

void GpuSemaphore::Create( const char* name, const bool isBinary )
{
#ifdef USE_VULKAN

	VkSemaphoreTypeCreateInfo timelineCreateInfo{};
	VkSemaphoreCreateInfo semaphoreInfo{ };
	semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	if( isBinary == false )
	{	
		timelineCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
		timelineCreateInfo.pNext = NULL;
		timelineCreateInfo.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
		timelineCreateInfo.initialValue = 0;

		semaphoreInfo.flags = VK_SEMAPHORE_TYPE_TIMELINE;
		semaphoreInfo.pNext = &timelineCreateInfo;
	}

	for( uint32_t i = 0; i < MaxFrameStates; ++i )
	{
		VK_CHECK_RESULT( vkCreateSemaphore( context.device, &semaphoreInfo, nullptr, &semaphores[ i ] ) );

		vk_SetObjectName( (uint64_t)semaphores[ i ], VK_OBJECT_TYPE_SEMAPHORE, vk_BuildObjectName( "Semaphore", name, i ).c_str() );
	}
#endif
}


void GpuSemaphore::Destroy()
{
#ifdef USE_VULKAN
	for ( uint32_t i = 0; i < MaxFrameStates; ++i ) {
		vkDestroySemaphore( context.device, semaphores[ i ], nullptr );
	}
#endif
}

#ifdef USE_VULKAN
VkSemaphore& GpuSemaphore::VkObject()
{
	return semaphores[ context.bufferId ];
}


VkSemaphore GpuSemaphore::GetVkObject() const
{
	return semaphores[ context.bufferId ];
}
#endif


void GpuFence::Create( const char* name )
{
#ifdef USE_VULKAN
	VkFenceCreateInfo fenceInfo{ };
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
	
	if ( vkCreateFence( context.device, &fenceInfo, nullptr, &fence ) != VK_SUCCESS ) {
		THROW_ERROR( "Failed to create fence!" );
	}
	vk_SetObjectName( (uint64_t)fence, VK_OBJECT_TYPE_FENCE, vk_BuildObjectName( "Fence", name ).c_str() );
#endif
}


void GpuFence::Destroy()
{
#ifdef USE_VULKAN
	vkDestroyFence( context.device, fence, nullptr );
#endif
}


void GpuFence::Wait()
{
	if ( fence != VK_NULL_HANDLE ) {
		vkWaitForFences( context.device, 1, &fence, VK_TRUE, UINT64_MAX );
		vkResetFences( context.device, 1, &fence );
	}
}

#ifdef USE_VULKAN
VkFence& GpuFence::VkObject()
{
	return fence;
}


VkFence GpuFence::GetVkObject() const
{
	return fence;
}
#endif