#include "../scene/assetManager.h" // FIXME: shouldn't expose this everywhere

#include "cmdContext.h"
#include "deviceContext.h"
#include "../render_core/gpuTimerPool.h"
#include "../render_core/renderer.h"
#include "../render_binding/pipeline.h"
#include "../render_resources/imageView.h"
#include "../render_state/frameBuffer.h"
#include "../render_binding/bindings.h"
#include "../render_tasks/imageShaderTask.h"

#define SHADER_STRUCTS_CPP
#include "../../shaders/gpuShared.h"

extern AssetManager g_assets;

/////////////////////////////////////////////
// Base Command Context
/////////////////////////////////////////////
VkCommandBuffer& CommandList::CommandBuffer()
{
	return commandBuffers[ context.bufferId ];
}


void CommandList::Begin()
{
#ifdef USE_VULKAN
	vkResetCommandBuffer( CommandBuffer(), 0 );

	waitSemaphores.clear();
	signalSemaphores.clear();
	waitTimelineSemaphores.clear();
	signalTimelineSemaphores.clear();

	VkCommandBufferBeginInfo beginInfo{ };
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = 0; // Optional
	beginInfo.pInheritanceInfo = nullptr; // Optional

	VK_CHECK_RESULT( vkBeginCommandBuffer( CommandBuffer(), &beginInfo ) );
#endif

	isOpen = true;
}


void CommandList::End()
{
#ifdef USE_VULKAN
	VK_CHECK_RESULT( vkEndCommandBuffer( CommandBuffer() ) );
#endif

	isOpen = false;
}


void CommandList::Create( const char* name, RenderContext* renderContext )
{
	m_renderContext = renderContext;

#ifdef USE_VULKAN
	// Pool creation
	{
		VkCommandPoolCreateInfo poolInfo{ };
		poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		poolInfo.queueFamilyIndex = context.queueFamilyIndices[ queueType ];
		poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
		VK_CHECK_RESULT( vkCreateCommandPool( context.device, &poolInfo, nullptr, &commandPool ) );

		vk_SetObjectName( (uint64_t)commandPool, VK_OBJECT_TYPE_COMMAND_POOL, vk_BuildObjectName( "CommandPool", name ).c_str() );
	}

	// Buffer creation
	{
		VkCommandBufferAllocateInfo allocInfo{ };
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.commandPool = commandPool;
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocInfo.commandBufferCount = static_cast<uint32_t>( MaxFrameStates );

		VK_CHECK_RESULT( vkAllocateCommandBuffers( context.device, &allocInfo, commandBuffers ) );

		for ( uint32_t i = 0; i < MaxFrameStates; i++ )
		{
			vkResetCommandBuffer( commandBuffers[ i ], 0 );
			vk_SetObjectName( (uint64_t)commandBuffers[ i ], VK_OBJECT_TYPE_COMMAND_BUFFER, vk_BuildObjectName( "CommandBuffer", name, i ).c_str() );
		}
	}
#endif
}


void CommandList::Destroy()
{
#ifdef USE_VULKAN
	vkFreeCommandBuffers( context.device, commandPool, static_cast<uint32_t>( MaxFrameStates ), commandBuffers );
	vkDestroyCommandPool( context.device, commandPool, nullptr );
#endif
}


void CommandList::Wait( GpuSemaphore* semaphore )
{
	waitSemaphores.push_back( semaphore );
}


void CommandList::Signal( GpuSemaphore* semaphore )
{
	signalSemaphores.push_back( semaphore );
}


void CommandList::Wait( GpuTimelineSemaphore* semaphore, uint64_t value )
{
	waitTimelineSemaphores.push_back( { semaphore, value } );
}


void CommandList::Signal( GpuTimelineSemaphore* semaphore, uint64_t value )
{
	signalTimelineSemaphores.push_back( { semaphore, value } );
}


void CommandList::MarkerBeginRegion( const char* pMarkerName, const vec4f& color )
{
	if ( context.debugMarkersEnabled )
	{
#ifdef USE_VULKAN
		VkDebugMarkerMarkerInfoEXT markerInfo = {};
		markerInfo.sType = VK_STRUCTURE_TYPE_DEBUG_MARKER_MARKER_INFO_EXT;
		markerInfo.color[ 0 ] = color[ 0 ];
		markerInfo.color[ 1 ] = color[ 1 ];
		markerInfo.color[ 2 ] = color[ 2 ];
		markerInfo.color[ 3 ] = color[ 3 ];
		markerInfo.pMarkerName = pMarkerName;
		context.fnCmdDebugMarkerBegin( CommandBuffer(), &markerInfo );
#endif
	}
}


void CommandList::MarkerEndRegion()
{
	if ( context.debugMarkersEnabled && ( context.fnCmdDebugMarkerEnd != nullptr ) )
	{
#ifdef USE_VULKAN
		context.fnCmdDebugMarkerEnd( CommandBuffer() );
#endif
	}
}


void CommandList::MarkerInsert( std::string markerName, const vec4f& color )
{
	if ( context.debugMarkersEnabled )
	{
#ifdef USE_VULKAN
		VkDebugMarkerMarkerInfoEXT markerInfo = {};
		markerInfo.sType = VK_STRUCTURE_TYPE_DEBUG_MARKER_MARKER_INFO_EXT;
		memcpy( markerInfo.color, &color[ 0 ], sizeof( float ) * 4 );
		markerInfo.pMarkerName = markerName.c_str();
		context.fnCmdDebugMarkerInsert( CommandBuffer(), &markerInfo );
#endif
	}
}


void CommandList::BeginTimestamp( const char* name )
{
#ifdef USE_VULKAN
	g_gpuTimerPool.BeginScope( this, context.bufferId, name );
#endif
}


void CommandList::EndTimestamp( const char* name )
{
#ifdef USE_VULKAN
	g_gpuTimerPool.EndScope( this, context.bufferId, name );
#endif
}


void CommandList::Submit( const GpuFence* fence )
{
#ifdef USE_VULKAN
	VkSubmitInfo submitInfo{ };
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

	std::vector<VkPipelineStageFlags> vk_waitStages;
	std::vector<VkSemaphore> vk_waitSemaphores;
	std::vector<uint64_t> vk_waitValues;
	vk_waitStages.reserve( waitSemaphores.size() + waitTimelineSemaphores.size() );
	vk_waitSemaphores.reserve( waitSemaphores.size() + waitTimelineSemaphores.size() );
	vk_waitValues.reserve( waitSemaphores.size() + waitTimelineSemaphores.size() );

	for( GpuSemaphore* semaphore : waitSemaphores )
	{
		assert( semaphore->waitStage != VK_PIPELINE_STAGE_NONE_KHR );
		vk_waitStages.push_back( semaphore->waitStage );
		vk_waitSemaphores.push_back( semaphore->GetVkObject() );
		vk_waitValues.push_back( 0 );
	}

	for( const timelineSemaphoreOp_t& op : waitTimelineSemaphores )
	{
		assert( op.semaphore->waitStage != VK_PIPELINE_STAGE_NONE_KHR );
		vk_waitStages.push_back( op.semaphore->waitStage );
		vk_waitSemaphores.push_back( op.semaphore->GetVkObject() );
		vk_waitValues.push_back( op.value );
	}

	std::vector<VkSemaphore> vk_signalSemaphores;
	std::vector<uint64_t> vk_signalValues;
	vk_signalSemaphores.reserve( signalSemaphores.size() + signalTimelineSemaphores.size() );
	vk_signalValues.reserve( signalSemaphores.size() + signalTimelineSemaphores.size() );

	for ( GpuSemaphore* semaphore : signalSemaphores )
	{
		vk_signalSemaphores.push_back( semaphore->GetVkObject() );
		vk_signalValues.push_back( 0 );
	}

	for ( const timelineSemaphoreOp_t& op : signalTimelineSemaphores )
	{
		vk_signalSemaphores.push_back( op.semaphore->GetVkObject() );
		vk_signalValues.push_back( op.value );
	}

	if ( vk_waitSemaphores.size() > 0 )
	{
		submitInfo.waitSemaphoreCount = static_cast<uint32_t>( vk_waitSemaphores.size() );
		submitInfo.pWaitSemaphores = vk_waitSemaphores.data();
		submitInfo.pWaitDstStageMask = vk_waitStages.data();
	}
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &CommandBuffer();

	if( vk_signalSemaphores.size() > 0 )
	{
		submitInfo.signalSemaphoreCount = static_cast<uint32_t>( vk_signalSemaphores.size() );
		submitInfo.pSignalSemaphores = vk_signalSemaphores.data();
	}

	// Only needed when a timeline semaphore is actually part of this submit -- values are ignored
	// by the driver for any binary semaphores mixed into the lists above, so when only binary
	// semaphores are present there's no need to chain this at all.
	VkTimelineSemaphoreSubmitInfo timelineInfo{};
	if( ( waitTimelineSemaphores.empty() == false ) || ( signalTimelineSemaphores.empty() == false ) )
	{
		timelineInfo.sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO;
		timelineInfo.waitSemaphoreValueCount = static_cast<uint32_t>( vk_waitValues.size() );
		timelineInfo.pWaitSemaphoreValues = vk_waitValues.data();
		timelineInfo.signalSemaphoreValueCount = static_cast<uint32_t>( vk_signalValues.size() );
		timelineInfo.pSignalSemaphoreValues = vk_signalValues.data();
		submitInfo.pNext = &timelineInfo;
	}

	VkFence vk_fence = ( fence != nullptr ) ? fence->GetVkObject() : VK_NULL_HANDLE;

	VkQueue vk_queue = ( queueType == QUEUE_COMPUTE ) ? context.computeContext : context.gfxContext;

	VK_CHECK_RESULT( vkQueueSubmit( vk_queue, 1, &submitInfo, vk_fence ) );
#endif
}


void CommandList::Dispatch( const Asset<GpuProgram>& progAsset, const ShaderBindParms& bindParms, const uint32_t x, const uint32_t y, const uint32_t z )
{
	Dispatch( progAsset, bindParms, nullptr, 0, x, y, z );
}


void CommandList::Dispatch( const Asset<GpuProgram>& progAsset, const ShaderBindParms& bindParms, const void* constants, const uint32_t constantsByteSize, const uint32_t x, const uint32_t y, const uint32_t z )
{
	assert( isOpen );
	assert( ( constantsByteSize % 4 ) == 0 );

	assert( progAsset.IsLoaded() );
	assert( progAsset.Handle() != INVALID_HDL );
	assert( progAsset.Get().type == pipelineType_t::COMPUTE );

	const hdl_t pipelineHdl = CreateComputePipeline( progAsset );

	pipelineObject_t* pipelineObject = nullptr;
	GetPipelineObject( pipelineHdl, &pipelineObject );

	assert( pipelineObject && pipelineObject->pipeline );

	std::string dbgName = "Dispatch( " + std::string( pipelineObject->dbgProgName ) + " )";

	MarkerBeginRegion( dbgName.c_str(), ColorToVector( ColorWhite ) );

#ifdef USE_VULKAN
	VkCommandBuffer cmdBuffer = CommandBuffer();
	VkDescriptorSet set[1] = { bindParms.GetVkObject() };

	if( ( constants != nullptr ) && ( constantsByteSize > 0 )  ) {
		vkCmdPushConstants( cmdBuffer, pipelineObject->pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, constantsByteSize, constants );
	}
	vkCmdBindPipeline( cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineObject->pipeline );
	vkCmdBindDescriptorSets( cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineObject->pipelineLayout, 0, 1, set, 0, 0 );

	vkCmdDispatch( cmdBuffer, x, y, z );
#endif
	MarkerEndRegion();
}


void Transition( CommandList* cmdList, const Image& image, gpuImageStateFlags_t current, gpuImageStateFlags_t next )
{
	Transition( cmdList, image, swapBuffering_t::SINGLE_FRAME, current, next );
}


void Transition( CommandList* cmdList, const Image& image, swapBuffering_t buffering, gpuImageStateFlags_t current, gpuImageStateFlags_t next )
{
	cmdList->MarkerBeginRegion( "Transition", ColorToVector( ColorWhite ) );

	imageSubResourceView_t subview {};
	subview.mipLevels = image.info.mipLevels;
	subview.arrayCount = image.info.layers;

#ifdef USE_VULKAN
	vk_TransitionImageLayout( cmdList->CommandBuffer(), &image, subview, buffering, current, next );
#endif

	cmdList->MarkerEndRegion();
}


void Transition( CommandList* cmdList, const GpuImage* gpuImage, swapBuffering_t buffering, gpuImageStateFlags_t current, gpuImageStateFlags_t next )
{
	cmdList->MarkerBeginRegion( "Transition", ColorToVector( ColorWhite ) );

	imageSubResourceView_t subview{};
	subview.mipLevels = gpuImage->GetInfo().mipLevels;
	subview.arrayCount = gpuImage->GetInfo().layers;

#ifdef USE_VULKAN
	vk_TransitionImageLayout( cmdList->CommandBuffer(), gpuImage, subview, buffering, current, next );
#endif

	cmdList->MarkerEndRegion();
}


void Transition( CommandList* cmdList, ImageView& imageView, gpuImageStateFlags_t current, gpuImageStateFlags_t next )
{
	Transition( cmdList, imageView, swapBuffering_t::SINGLE_FRAME, current, next );
}


void Transition( CommandList* cmdList, ImageView& imageView, swapBuffering_t buffering, gpuImageStateFlags_t current, gpuImageStateFlags_t next )
{
	cmdList->MarkerBeginRegion( "Transition Image View", ColorToVector( ColorWhite ) );

#ifdef USE_VULKAN
	vk_TransitionImageLayout( cmdList->CommandBuffer(), &imageView, imageView.subResourceView, buffering, current, next );
#endif

	cmdList->MarkerEndRegion();
}


void GenerateMipmaps( CommandList* cmdList, Image& image )
{
	cmdList->MarkerBeginRegion( "GenerateMips", ColorToVector( ColorWhite ) );

#ifdef USE_VULKAN
	vk_GenerateMipmaps( cmdList->CommandBuffer(), &image );
#endif

	cmdList->MarkerEndRegion();
}


void CopyImage( CommandList* cmdList, Image& src, Image& dst )
{
	cmdList->MarkerBeginRegion( "CopyImage", ColorToVector( ColorWhite ) );

#ifdef USE_VULKAN
	vk_CopyImage( cmdList->CommandBuffer(), src, dst );
#endif

	cmdList->MarkerEndRegion();
}


void CopyImage( CommandList* cmdList, Image& src, const copyImageParms_t& srcParms, Image& dst, const copyImageParms_t& dstParms )
{
	cmdList->MarkerBeginRegion( "CopyImage", ColorToVector( ColorWhite ) );

#ifdef USE_VULKAN
	vk_CopyImage( cmdList->CommandBuffer(), &src, srcParms, &dst, dstParms );
#endif

	cmdList->MarkerEndRegion();
}


void ResolveImage( CommandList* cmdList, const resolveImageInfo_t& info )
{
	cmdList->MarkerBeginRegion( "ResolveImage", ColorToVector( ColorWhite ) );

#ifdef USE_VULKAN
	vk_ResolveImage( cmdList->CommandBuffer(), info );
#endif

	cmdList->MarkerEndRegion();
}


void UploadImageData( CommandList* cmdList, Image& image, imageSubResourceView_t& subView, GpuBuffer& buffer )
{
	cmdList->MarkerBeginRegion( "UploadImageData", ColorToVector( ColorWhite ) );

	copyImageParms_t copyParms{};

	copyParms.x = 0;
	copyParms.y = 0;
	copyParms.z = 0;
	copyParms.width = image.info.width;
	copyParms.height = image.info.height;
	copyParms.depth = 1;
	copyParms.baseArray = subView.baseArray;
	copyParms.arrayCount = subView.arrayCount;
	copyParms.baseMip = subView.baseMip;
	copyParms.mipLevels = subView.mipLevels;

#ifdef USE_VULKAN
	vk_UploadImageData( cmdList->CommandBuffer(), &image, copyParms, buffer );
#endif

	cmdList->MarkerEndRegion();
}


void FlushGPU()
{
#ifdef USE_VULKAN
	vkDeviceWaitIdle( context.device );
#endif
}


#ifdef USE_VULKAN_RTX
void CommandList::TraceRays( const hdl_t rtPipelineHdl, const ShaderBindParms& bindParms, const uint32_t width, const uint32_t height )
{
	TraceRays( rtPipelineHdl, bindParms, nullptr, 0, width, height );
}


void CommandList::TraceRays( const hdl_t rtPipelineHdl, const ShaderBindParms& bindParms, const void* constants, const uint32_t constantsByteSize, const uint32_t width, const uint32_t height )
{
	assert( isOpen );
	assert( ( constantsByteSize % 4 ) == 0 );

	pipelineObject_t* pipelineObject = nullptr;
	GetPipelineObject( rtPipelineHdl, &pipelineObject );
	assert( pipelineObject && pipelineObject->pipeline );

	MarkerBeginRegion( "TraceRays", ColorToVector( ColorWhite ) );

	VkCommandBuffer cmdBuffer = CommandBuffer();

	const VkShaderStageFlags rtStages =	( VK_SHADER_STAGE_RAYGEN_BIT_KHR
									|	VK_SHADER_STAGE_MISS_BIT_KHR
									|	VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR
									|	VK_SHADER_STAGE_ANY_HIT_BIT_KHR
									|	VK_SHADER_STAGE_INTERSECTION_BIT_KHR );

	if ( ( constants != nullptr ) && ( constantsByteSize > 0 ) )
	{
		vkCmdPushConstants( cmdBuffer, pipelineObject->pipelineLayout, rtStages, 0, constantsByteSize, constants );
	}

	vkCmdBindPipeline( cmdBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipelineObject->pipeline );

	const VkDescriptorSet sets[ 2 ] = {
		m_renderContext->globalParms->GetVkObject(),
		bindParms.GetVkObject()
	};
	vkCmdBindDescriptorSets( cmdBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipelineObject->pipelineLayout, 0, 2, sets, 0, 0 );

	const vk_shaderBindTable_t& sbt = pipelineObject->state.rtState.sbt;
	context.vkCmdTraceRaysKHR(
		cmdBuffer,
		&sbt.rgenRegion,
		&sbt.missRegion,
		&sbt.hitGroupRegion,
		&sbt.callableRegion,
		width, height, 1 );

	MarkerEndRegion();
}
#endif
