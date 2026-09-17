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
	};

	struct instanceData_t
	{
		uint32_t	instanceIndex;
		mat4x4f		transform;
	};

	// Pending geometry accumulated by AddGeometry(), consumed by BuildPendingGeometry()
	std::vector<VkAccelerationStructureGeometryKHR>			m_geometry;
	std::vector<VkAccelerationStructureBuildRangeInfoKHR>	m_rangeInfo;
	std::vector<gpuRtSurface_t>								m_pendingSurfaceInfos;	// Parallel to m_geometry

	std::vector<instanceData_t>		m_pendingInstances;

	std::vector<blasEntry_t>		m_blasEntries;
	std::vector<gpuRtSurface_t>		m_cpuSurfaceInfos;	// CPU mirror, one entry per blasEntry; uploaded to m_rtSurfaceInfoBuf
	GpuBuffer						m_blasScratch;		// Shared scratch for the current build batch only
	GpuBuffer						m_rtSurfaceInfoBuf;	// Per-BLAS vertex/index offsets for closest-hit attribute fetch

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
	void						UpdateSurfaceInstance( uint32_t surfaceUploadId, const mat4x4f& transform );
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
