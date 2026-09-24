#pragma once

#include <cstdint>

#include "../render_resources/gpuBuffer.h"
#include "../render_state/cmdContext.h"
#include "../render_core/GpuSync.h"

#define SHADER_STRUCTS_CPP
#include "../../shaders/gpuShared.h"

class RenderContext;
class ResourceContext;
class Model;
class Material;
class RenderUploader;
class GpuAccelerationStructure;

struct surfaceUpload_t
{
	surfaceUpload_t() : vertexCount( 0 ), indexCount( 0 ), vertexOffset( 0 ), firstIndex( 0 )
	{}

	uint32_t					vertexCount;
	uint32_t					indexCount;
	uint32_t					vertexOffset;
	uint32_t					firstIndex;
};


// Bundle of all resources needed to represent geometry on the GPU
class GeometryContext
{
private:

	using surfUploadArray_t = Array<surfaceUpload_t, MaxSurfaces* MaxViews>;

	GpuBuffer			stagingBuffer;
	uint32_t			vbBufElements = 0;
	uint32_t			ibBufElements = 0;

public:
	GpuBuffer			vb;
	GpuBuffer			ib;
	surfUploadArray_t	surfUploads;

	friend RenderUploader;
};


class RenderUploader
{
private:
	uint32_t					imageFreeSlot;
	GpuBuffer					textureStagingBuffer;
	GeometryContext				geometry;
	UploadCmdList				commands;
	GpuSemaphore				finishedSemaphore;
	GpuFence					uploadFence;
	GpuAccelerationStructure*	accelerationStructure;

	RenderContext*				renderContext;
	ResourceContext*			resourceContext;

	std::set<hdl_t>				uploadImages;
	std::set<hdl_t>				refreshImages;
	std::set<hdl_t>				uploadMaterials;

	using materialBufferArray_t = Array<gpuMaterial_t, MaxMaterials>;
	materialBufferArray_t	materialBuffer; // This is a holdover from older code and can be removed

#ifdef USE_VULKAN
	void				CopyGpuBuffer( CommandList* cmdList, GpuBuffer& srcBuffer, GpuBuffer& dstBuffer, VkBufferCopy copyRegion );
#endif

public:

	void				Boot( RenderContext* context, ResourceContext* resources );
	void				Shutdown();

	void				OnReboot();
	void				OnFrameBegin( GpuTimelineSemaphore* frameCompleteTimeline, uint64_t waitValue );
	void				Upload();

	void				QueueImageUpload( Asset<Image>& imageAsset );
	void				QueueMaterialUpload( Asset<Material>& materialAsset );
	void				QueueModelUpload( Asset<Model>& model );

	void				UpdateTextureData( CommandList* cmdList );
	void				UploadTextures( CommandList* cmdList );
	void				UpdateGpuMaterials();
	void				UploadModelsToGPU( CommandList* cmdList );

	inline const GeometryContext* GetGeometry() const
	{
		return &geometry;
	}

	inline GpuSemaphore* GetFinishedSemaphore()
	{
		return &finishedSemaphore;
	}

	inline GpuAccelerationStructure* GetAccelerationStructure()
	{
		return accelerationStructure;
	}
};