#pragma once

#include "../globals/common.h"
#include "../render_core/renderResource.h"
#include "gpuBuffer.h"

#define SHADER_STRUCTS_CPP
#include "../../shaders/gpuShared.h"

class CommandList;
class GeometryContext;

struct surfaceUpload_t;

struct rtSurfaceInfo_t
{
	const char*				name;
	const GeometryContext*	geometry;
	const surfaceUpload_t*	surface;
};

class GpuAccelerationStructure : public RenderResource
{
private:

#ifdef USE_VULKAN_RTX
	struct blasEntry_t
	{
		GpuBuffer					storage;	// Per-BLAS; independent so entries survive incremental additions
		GpuBufferView				scratchView;
		VkAccelerationStructureKHR	handle = VK_NULL_HANDLE;
		uint32_t					vertexOffset = 0;	// Static geometry data, shared by every instance referencing this BLAS
		uint32_t					firstIndex = 0;
	};


	struct blasSurfaceInfo_t
	{
		uint32_t	vertexOffset;
		uint32_t	firstIndex;
	};

	struct rtSurfInstance_t			// RT analogue to `drawSurfInstance_t`
	{
		uint32_t	surfId;			// Index to BLAS surface
		uint32_t	materialId;
		uint32_t	diffuseIblCubeId;
		uint32_t	envCubeId;
		mat4x4f		modelMatrix;
	};

	// Pending geometry accumulated by AddGeometry(), consumed by BuildPendingGeometry()
	std::vector<VkAccelerationStructureGeometryKHR>			m_geometry;
	std::vector<VkAccelerationStructureBuildRangeInfoKHR>	m_rangeInfo;
	std::vector<blasSurfaceInfo_t>							m_pendingSurfaceInfos;	// Parallel to m_geometry

	// Per-instance, rebuilt fresh every frame by CommitRayTraceInstances() -> UpdateSurfaceInstance()
	std::vector<rtSurfInstance_t>		m_pendingInstances;

	std::vector<blasEntry_t>		m_blasEntries;
	GpuBuffer						m_blasScratch;		// Shared scratch for the current build batch only
	GpuBuffer						m_rtSurfaceInfoBuf;	// One gpuRtSurface_t per TLAS instance. Rebuilt every frame

	GpuBuffer						m_tlasInstanceBuf;	// GPU instance buffer
	GpuBuffer						m_tlasStorage;
	GpuBuffer						m_tlasScratch;
	VkAccelerationStructureKHR		m_tlas = VK_NULL_HANDLE;
	uint32_t						m_tlasInstanceCount = 0;

	const char*			m_name     = nullptr;
	resourceLifeTime_t	m_lifetime = {};

	void				Cleanup();
#endif

public:
	void						Create( const char* name, resourceLifeTime_t lifetime );
	void						AddGeometry( CommandList* cmdList, const rtSurfaceInfo_t& surfaceInfo );
	void						BuildPendingGeometry( CommandList* cmdList );
	void						UpdateSurfaceInstance( uint32_t surfaceUploadId, uint32_t materialId, uint32_t diffuseIblCubeId, uint32_t envCubeId, const mat4x4f& transform );
	void						Update( CommandList* cmdList );
	void						Destroy() override;

#ifdef USE_VULKAN_RTX
	VkDeviceAddress				GetBlasDeviceAddress( uint32_t index ) const;
	uint32_t					GetBlasCount() const { return static_cast<uint32_t>( m_blasEntries.size() ); }
	bool						IsBuilt() const { return m_tlas != VK_NULL_HANDLE; }
	const GpuBuffer*			GetSurfaceInfoBuffer() const { return &m_rtSurfaceInfoBuf; }

	VkDeviceAddress				GetDeviceAddress() const;
	VkAccelerationStructureKHR	GetVkObject() const;
#endif
};
