#pragma once



#include "../globals/common.h"
#include "../render_core/GpuSync.h"

class ShaderBindParms;
class GpuBuffer;
class RenderContext;
class DrawPass;
class ImageView;
class GpuImage;
class FrameBuffer;
struct imageSubResourceView_t;
struct copyImageParms_t;
struct resolveImageInfo_t;

enum gpuImageStateFlags_t : uint8_t;

enum pipelineQueue_t
{
	QUEUE_UNKNOWN,
	QUEUE_GRAPHICS,
	QUEUE_PRESENT,
	QUEUE_COMPUTE,
	QUEUE_COUNT,
};


template<class T>
class optional {

	std::pair<T, bool> option;

public:

	optional()
	{
		option.second = false;
	}

	bool has_value()
	{
		return option.second;
	}

	void set_value( const T& value_ )
	{
		option.first = value_;
		option.second = true;
	}

	T value()
	{
		return option.first;
	}
};


struct QueueFamilyIndices
{
	optional<uint32_t> graphicsFamily;
	optional<uint32_t> presentFamily;
	optional<uint32_t> computeFamily;

	bool IsComplete() {
		return	graphicsFamily.has_value() &&
			presentFamily.has_value() &&
			computeFamily.has_value();
	}
};


class CommandList
{
protected:
	pipelineQueue_t				queueType;
	bool						isOpen;

private:
	struct timelineSemaphoreOp_t
	{
		GpuTimelineSemaphore*	semaphore;
		uint64_t				value;
	};

	std::vector<GpuSemaphore*>				waitSemaphores;
	std::vector<GpuSemaphore*>				signalSemaphores;
	std::vector<timelineSemaphoreOp_t>		waitTimelineSemaphores;
	std::vector<timelineSemaphoreOp_t>		signalTimelineSemaphores;
#ifdef USE_VULKAN
	VkCommandPool				commandPool;
	VkCommandBuffer				commandBuffers[ MaxFrameStates ];
#endif
	RenderContext*				m_renderContext;

public:
	CommandList()
	{
#ifdef USE_VULKAN
		commandPool = VK_NULL_HANDLE;
		for( uint32_t i = 0; i < MaxFrameStates; ++i ) {
			commandBuffers[ i ] = VK_NULL_HANDLE;
		}

		queueType = QUEUE_UNKNOWN;
		isOpen = false;
#endif
	}
#ifdef USE_VULKAN
	VkCommandBuffer&			CommandBuffer();
#endif
	void						Begin();
	void						End();
	void						Create( const char* name, RenderContext* context );
	void						Destroy();
	void						MarkerBeginRegion( const char* pMarkerName, const vec4f& color );
	void						MarkerEndRegion();
	void						MarkerInsert( std::string markerName, const vec4f& color );
	void						BeginTimestamp( const char* name );
	void						EndTimestamp( const char* name );
	void						Wait( GpuSemaphore* semaphore );
	void						Signal( GpuSemaphore* semaphore );
	void						Wait( GpuTimelineSemaphore* semaphore, uint64_t value );
	void						Signal( GpuTimelineSemaphore* semaphore, uint64_t value );
	void						Submit( const GpuFence* fence = nullptr );
	void						Dispatch( const Asset<GpuProgram>& progAsset, const ShaderBindParms& bindParms, const uint32_t x, const uint32_t y, const uint32_t z );
	void						Dispatch( const Asset<GpuProgram>& progAsset, const ShaderBindParms& bindParms, const void* constants, const uint32_t constantsByteSize, const uint32_t x, const uint32_t y, const uint32_t z );
#ifdef USE_VULKAN_RTX
	void						TraceRays( hdl_t rtPipelineHdl, const ShaderBindParms& bindParms, uint32_t width, uint32_t height );
	void						TraceRays( hdl_t rtPipelineHdl, const ShaderBindParms& bindParms, const void* constants, uint32_t constantsByteSize, uint32_t width, uint32_t height );
#endif


	static uint32_t DispatchDim( const uint32_t dim, const uint32_t groupSize )
	{
		return 	( dim + groupSize - 1 ) / groupSize;
	}

	inline RenderContext*	GetRenderContext()
	{
		return m_renderContext;
	}

	inline const RenderContext* GetRenderContext() const
	{
		return m_renderContext;
	}
};


class GfxCmdList : public CommandList
{
public:
	GpuSemaphore	presentSemaphore;
	GpuSemaphore	renderFinishedSemaphore;
	GpuFence		frameFence[ MaxFrameStates ];

	GfxCmdList()
	{
		queueType = QUEUE_GRAPHICS;
	}
};


class ComputeCmdList : public CommandList
{
public:
	GpuSemaphore	semaphore;

	ComputeCmdList()
	{
		queueType = QUEUE_COMPUTE;
	}
};


class UploadCmdList : public CommandList
{
private:
	using CommandList::Dispatch;

public:
	UploadCmdList()
	{
		queueType = QUEUE_GRAPHICS;
	}
};


void Transition( CommandList* cmdList, const Image& image, gpuImageStateFlags_t current, gpuImageStateFlags_t next );
void Transition( CommandList* cmdList, const Image& image, swapBuffering_t buffering, gpuImageStateFlags_t current, gpuImageStateFlags_t next );
void Transition( CommandList* cmdList, const GpuImage* gpuImage, swapBuffering_t buffering, gpuImageStateFlags_t current, gpuImageStateFlags_t next );
void GenerateMipmaps( CommandList* cmdList, Image& image );
void CopyImage( CommandList* cmdList, Image& src, Image& dst );
void CopyImage( CommandList* cmdList, Image& src, const copyImageParms_t& srcParms, Image& dst, const copyImageParms_t& dstParms );
void ResolveImage( CommandList* cmdList, const resolveImageInfo_t& info );
void UploadImageData( CommandList* cmdList, Image& image, imageSubResourceView_t& subView, GpuBuffer& buffer );
void FlushGPU();