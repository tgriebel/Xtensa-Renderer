#include "gpuAccelerationStructure.h"
#include "../render_state/cmdContext.h"
#include "../render_state/deviceContext.h"
#include "../render_core/renderUploader.h"
#include "../render_core/log.h"

#include <SysCore/common.h>
using namespace SysCore;

extern DeviceContext context;


#ifdef USE_VULKAN_RTX


void GpuAccelerationStructure::Create( const char* name, resourceLifeTime_t lifetime )
{
	m_name = name;
	m_lifetime = lifetime;
	RenderResource::Create( resourceType_t::ACCELERATION_STRUCTURE, lifetime );
}


void GpuAccelerationStructure::AddGeometry( CommandList* cmdList, const rtSurfaceInfo_t& surfaceInfo )
{
	const GpuBuffer& vb = surfaceInfo.geometry->vb;
	const GpuBuffer& ib = surfaceInfo.geometry->ib;

	// Temp sanity while RT is being stood up
	assert( vb.GetElementSize() == sizeof( vsInput_t ) );
	assert( ib.GetElementSize() == sizeof( uint32_t ) );

	VkAccelerationStructureGeometryTrianglesDataKHR triangles{};
	triangles.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
	triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
	triangles.vertexData.deviceAddress = vb.GetDeviceAddress();
	triangles.vertexStride = vb.GetElementSize();
	triangles.maxVertex = surfaceInfo.surface->vertexOffset + surfaceInfo.surface->vertexCount - 1;
	triangles.indexType = VK_INDEX_TYPE_UINT32;
	triangles.indexData.deviceAddress = ib.GetDeviceAddress();

	VkAccelerationStructureGeometryKHR geometry{};
	geometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
	geometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
	geometry.geometry.triangles = triangles;
	geometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;

	m_geometry.push_back( geometry );

	VkAccelerationStructureBuildRangeInfoKHR rangeInfo{};
	rangeInfo.primitiveCount = surfaceInfo.surface->indexCount / 3;
	rangeInfo.primitiveOffset = surfaceInfo.surface->firstIndex * sizeof( uint32_t );
	rangeInfo.firstVertex = surfaceInfo.surface->vertexOffset;
	rangeInfo.transformOffset = 0;

	m_rangeInfo.push_back( rangeInfo );

	blasSurfaceInfo_t geomInfo{};
	geomInfo.vertexOffset = surfaceInfo.surface->vertexOffset;
	geomInfo.firstIndex   = surfaceInfo.surface->firstIndex;
	m_pendingSurfaceInfos.push_back( geomInfo );
}


void GpuAccelerationStructure::Cleanup()
{
	for ( blasEntry_t& entry : m_blasEntries )
	{
		if ( entry.handle != VK_NULL_HANDLE )
		{
			context.vkDestroyAccelerationStructureKHR( context.device, entry.handle, nullptr );
			entry.handle = VK_NULL_HANDLE;
		}
		if ( entry.storage.GetMaxSize() > 0 ) {
			entry.storage.Destroy();
		}
	}
	m_blasEntries.clear();
	m_pendingInstances.clear();

	if ( m_blasScratch.GetMaxSize() > 0 ) {
		m_blasScratch.Destroy();
	}
	if ( m_rtSurfaceInfoBuf.GetMaxSize() > 0 ) {
		m_rtSurfaceInfoBuf.Destroy();
	}

	if ( m_tlas != VK_NULL_HANDLE )
	{
		context.vkDestroyAccelerationStructureKHR( context.device, m_tlas, nullptr );
		m_tlas = VK_NULL_HANDLE;
	}
	if ( m_tlasStorage.GetMaxSize() > 0 ) {
		m_tlasStorage.Destroy();
	}
	if ( m_tlasScratch.GetMaxSize() > 0 ) {
		m_tlasScratch.Destroy();
	}
	if ( m_tlasInstanceBuf.GetMaxSize() > 0 ) {
		m_tlasInstanceBuf.Destroy();
	}
}


void GpuAccelerationStructure::BuildPendingGeometry( CommandList* cmdList )
{
	const uint32_t newCount = static_cast<uint32_t>( m_geometry.size() );

	if ( newCount == 0 ) {
		return;
	}

	LogMsg( "Vulkan", logSeverity_t::Info, "Building RT BLAS (%u new, %zu existing)", newCount, m_blasEntries.size() );

	// TODO: query from device properties
	// VkPhysicalDeviceAccelerationStructurePropertiesKHR::minAccelerationStructureScratchOffsetAlignment
	constexpr uint32_t scratchAlignment = 128;

	std::vector<VkAccelerationStructureBuildGeometryInfoKHR> buildInfos( newCount );
	std::vector<VkAccelerationStructureBuildSizesInfoKHR> allSizes( newCount );
	std::vector<const VkAccelerationStructureBuildRangeInfoKHR*> pRangeInfos( newCount );

	// 1: Query sizes for the pending BLASes, accumulate scratch requirement
	{
		uint32_t totalScratchSize = 0;
		for( uint32_t i = 0; i < newCount; ++i )
		{
			const uint32_t primCount = m_rangeInfo[ i ].primitiveCount;

			buildInfos[ i ] = {};
			buildInfos[ i ].sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
			buildInfos[ i ].type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
			buildInfos[ i ].flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
			buildInfos[ i ].geometryCount = 1;
			buildInfos[ i ].pGeometries = &m_geometry[ i ];

			allSizes[ i ].sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
			context.vkGetAccelerationStructureBuildSizesKHR( context.device,
				VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
				&buildInfos[ i ], &primCount, &allSizes[ i ] );

			totalScratchSize += Align( (uint64_t)allSizes[ i ].buildScratchSize, (uint64_t)scratchAlignment );
		}

		// Replace the previous batch's scratch with one sized for this batch
		if( m_blasScratch.GetMaxSize() > 0 ) {
			m_blasScratch.Destroy();
		}
		m_blasScratch.Create( m_name, swapBuffering_t::SINGLE_FRAME, m_lifetime, totalScratchSize, 1, bufferType_t::STORAGE, bufferFlags_t::RT_VISIBLE );
	}

	// 2: Create a per-BLAS storage buffer and AS handle for each pending entry
	m_blasEntries.reserve( m_blasEntries.size() + newCount );
	uint32_t scratchOffset = 0;
	for ( uint32_t i = 0; i < newCount; ++i )
	{
		blasEntry_t& entry = m_blasEntries.emplace_back();
		const VkAccelerationStructureBuildSizesInfoKHR& sizes = allSizes[ i ];

		// Each BLAS gets its own storage buffer so existing BLASes are unaffected by future additions
		entry.storage.Create( m_name, swapBuffering_t::SINGLE_FRAME, m_lifetime,
			static_cast<uint32_t>( sizes.accelerationStructureSize ), 1, bufferType_t::ACCELERATION_STRUCTURE );

		VkAccelerationStructureCreateInfoKHR createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
		createInfo.buffer = entry.storage.GetVkObject();
		createInfo.offset = 0;
		createInfo.size = sizes.accelerationStructureSize;
		createInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;

		VK_CHECK_RESULT( context.vkCreateAccelerationStructureKHR( context.device, &createInfo, nullptr, &entry.handle ) );

		entry.scratchView = m_blasScratch.GetBufferView( scratchOffset, static_cast<uint32_t>( sizes.buildScratchSize ) );
		scratchOffset += Align( (uint64_t)sizes.buildScratchSize, (uint64_t)scratchAlignment );

		buildInfos[ i ].dstAccelerationStructure = entry.handle;
		buildInfos[ i ].scratchData.deviceAddress = entry.scratchView.GetDeviceAddress();
		pRangeInfos[ i ] = &m_rangeInfo[ i ];

		entry.vertexOffset = m_pendingSurfaceInfos[ i ].vertexOffset;
		entry.firstIndex   = m_pendingSurfaceInfos[ i ].firstIndex;
	}

	// 3: Barrier: vertex/index data was written by vkCmdCopyBuffer
	{
		VkMemoryBarrier geometryUploadBarrier{};
		geometryUploadBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
		geometryUploadBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		geometryUploadBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		vkCmdPipelineBarrier( cmdList->CommandBuffer(),
			VK_PIPELINE_STAGE_TRANSFER_BIT,
			VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
			0, 1, &geometryUploadBarrier, 0, nullptr, 0, nullptr );
	}
	// 4: Build new BLASes
	{
		context.vkCmdBuildAccelerationStructuresKHR( cmdList->CommandBuffer(), newCount, buildInfos.data(), pRangeInfos.data() );
	}

	// 5: Barrier — new BLAS writes must be visible before the next TLAS build reads them
	{
		VkMemoryBarrier blasBarrier{};
		blasBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
		blasBarrier.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
		blasBarrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;
		vkCmdPipelineBarrier( cmdList->CommandBuffer(),
			VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
			VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
			0, 1, &blasBarrier, 0, nullptr, 0, nullptr );
	}
	m_pendingSurfaceInfos.clear();
	m_geometry.clear();
	m_rangeInfo.clear();
}


void GpuAccelerationStructure::UpdateSurfaceInstance( uint32_t surfaceUploadId, uint32_t materialId, uint32_t diffuseIblCubeId, uint32_t envCubeId, const mat4x4f& transform )
{
	if ( surfaceUploadId < static_cast<uint32_t>( m_blasEntries.size() ) ) {
		m_pendingInstances.push_back( { surfaceUploadId, materialId, diffuseIblCubeId, envCubeId, transform } );
	}
}


void GpuAccelerationStructure::Update( CommandList* cmdList )
{
	const uint32_t count = static_cast<uint32_t>( m_pendingInstances.size() );

	if ( count == 0 ) {
		return;
	}

	const bool fullRebuild = ( m_tlas == VK_NULL_HANDLE ) || ( count != m_tlasInstanceCount );

	std::vector<VkAccelerationStructureInstanceKHR> vkInstances( count );
	std::vector<gpuRtSurface_t> surfaceInfos( count );
	for ( uint32_t i = 0; i < count; ++i )
	{
		const rtSurfInstance_t& src = m_pendingInstances[ i ];
		assert( src.surfId < static_cast<uint32_t>( m_blasEntries.size() ) );
		const blasEntry_t& blas = m_blasEntries[ src.surfId ];

		VkAccelerationStructureInstanceKHR& inst = vkInstances[ i ];
		inst = {};

		for ( int32_t row = 0; row < 3; ++row ) {
			for ( int32_t col = 0; col < 4; ++col ) {
				inst.transform.matrix[ row ][ col ] = src.modelMatrix[ row ][ col ];
			}
		}

		VkAccelerationStructureDeviceAddressInfoKHR addressInfo{};
		addressInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
		addressInfo.accelerationStructure = m_blasEntries[ src.surfId ].handle;
		const uint64_t blasAddress = context.vkGetAccelerationStructureDeviceAddressKHR( context.device, &addressInfo );

		inst.instanceCustomIndex = i;
		inst.mask = 0xFF;
		inst.instanceShaderBindingTableRecordOffset = 0;
		inst.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
		inst.accelerationStructureReference = blasAddress;

		surfaceInfos[ i ].vertexOffset = blas.vertexOffset;
		surfaceInfos[ i ].firstIndex = blas.firstIndex;
		surfaceInfos[ i ].materialId = src.materialId;
		surfaceInfos[ i ].diffuseIblCubeId = src.diffuseIblCubeId;
		surfaceInfos[ i ].envCubeId = src.envCubeId;
		surfaceInfos[ i ].pad0 = 0;
	}

	// Rebuilt every frame
	if ( fullRebuild )
	{
		if ( m_rtSurfaceInfoBuf.GetMaxSize() > 0 ) {
			m_rtSurfaceInfoBuf.Destroy();
		}
		m_rtSurfaceInfoBuf.Create( m_name, swapBuffering_t::SINGLE_FRAME, m_lifetime,
			count, sizeof( gpuRtSurface_t ), bufferType_t::STORAGE );
	}
	m_rtSurfaceInfoBuf.SetPos( 0 );
	m_rtSurfaceInfoBuf.CopyData( surfaceInfos.data(), count * sizeof( gpuRtSurface_t ) );

	// Query sizes
	VkAccelerationStructureGeometryInstancesDataKHR instancesData{};
	instancesData.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
	instancesData.arrayOfPointers = VK_FALSE;

	VkAccelerationStructureGeometryKHR tlasGeometry{};
	tlasGeometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
	tlasGeometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
	tlasGeometry.geometry.instances = instancesData;

	VkAccelerationStructureBuildGeometryInfoKHR tlasBuildInfo{};
	tlasBuildInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
	tlasBuildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
	tlasBuildInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR
		| VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR;
	tlasBuildInfo.geometryCount = 1;
	tlasBuildInfo.pGeometries = &tlasGeometry;

	VkAccelerationStructureBuildSizesInfoKHR tlasSizes{};
	tlasSizes.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
	context.vkGetAccelerationStructureBuildSizesKHR( context.device,
		VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
		&tlasBuildInfo, &count, &tlasSizes );

	if ( fullRebuild )
	{
		if ( m_tlas != VK_NULL_HANDLE ) {
			context.vkDestroyAccelerationStructureKHR( context.device, m_tlas, nullptr );
			m_tlas = VK_NULL_HANDLE;
		}
		if( m_tlasStorage.GetMaxSize() > 0 ) {
			m_tlasStorage.Destroy();
		}
		if( m_tlasScratch.GetMaxSize() > 0 ) {
			m_tlasScratch.Destroy();
		}
		if ( m_tlasInstanceBuf.GetMaxSize() > 0 ) {
			m_tlasInstanceBuf.Destroy();
		}

		m_tlasInstanceBuf.Create( m_name, swapBuffering_t::SINGLE_FRAME, m_lifetime,
			count, sizeof( VkAccelerationStructureInstanceKHR ), bufferType_t::TLAS_INSTANCE_DATA );

		// Size scratch to cover both build and update so it can be reused every frame
		const uint64_t scratchSize = std::max( tlasSizes.buildScratchSize, tlasSizes.updateScratchSize );

		m_tlasScratch.Create( m_name, swapBuffering_t::SINGLE_FRAME, m_lifetime,
			static_cast<uint32_t>( scratchSize ), 1, bufferType_t::STORAGE, bufferFlags_t::RT_VISIBLE );

		m_tlasStorage.Create( m_name, swapBuffering_t::SINGLE_FRAME, m_lifetime,
			static_cast<uint32_t>( tlasSizes.accelerationStructureSize ), 1, bufferType_t::ACCELERATION_STRUCTURE );

		VkAccelerationStructureCreateInfoKHR tlasCreateInfo{};
		tlasCreateInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
		tlasCreateInfo.buffer = m_tlasStorage.GetVkObject();
		tlasCreateInfo.offset = 0;
		tlasCreateInfo.size = tlasSizes.accelerationStructureSize;
		tlasCreateInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
		VK_CHECK_RESULT( context.vkCreateAccelerationStructureKHR( context.device, &tlasCreateInfo, nullptr, &m_tlas ) );

		tlasBuildInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
		tlasBuildInfo.srcAccelerationStructure = VK_NULL_HANDLE;
	}
	else
	{
		tlasBuildInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_UPDATE_KHR;
		tlasBuildInfo.srcAccelerationStructure = m_tlas;
	}

	m_tlasInstanceBuf.SetPos( 0 );
	m_tlasInstanceBuf.CopyData( vkInstances.data(), vkInstances.size() * sizeof( VkAccelerationStructureInstanceKHR ) );
	tlasGeometry.geometry.instances.data.deviceAddress = m_tlasInstanceBuf.GetDeviceAddress();

	tlasBuildInfo.dstAccelerationStructure = m_tlas;
	tlasBuildInfo.scratchData.deviceAddress = m_tlasScratch.GetDeviceAddress();

	VkAccelerationStructureBuildRangeInfoKHR tlasRangeInfo{};
	tlasRangeInfo.primitiveCount = count;
	const VkAccelerationStructureBuildRangeInfoKHR* pTlasRangeInfo = &tlasRangeInfo;

	context.vkCmdBuildAccelerationStructuresKHR( cmdList->CommandBuffer(), 1, &tlasBuildInfo, &pTlasRangeInfo );

	m_tlasInstanceCount = count;
	m_pendingInstances.clear();

	// Barrier
	{
		VkMemoryBarrier tlasBarrier{};
		tlasBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
		tlasBarrier.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
		tlasBarrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;
		vkCmdPipelineBarrier( cmdList->CommandBuffer(),
			VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
			VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
			0, 1, &tlasBarrier, 0, nullptr, 0, nullptr );
	}
}


VkDeviceAddress GpuAccelerationStructure::GetDeviceAddress() const
{
	VkAccelerationStructureDeviceAddressInfoKHR addressInfo{};
	addressInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
	addressInfo.accelerationStructure = m_tlas;
	return context.vkGetAccelerationStructureDeviceAddressKHR( context.device, &addressInfo );
}


VkAccelerationStructureKHR GpuAccelerationStructure::GetVkObject() const
{
	return m_tlas;
}


void GpuAccelerationStructure::Destroy()
{
	Cleanup();
}

#else // !USE_VULKAN_RTX

void GpuAccelerationStructure::Create( const char*, resourceLifeTime_t ) {}
void GpuAccelerationStructure::AddGeometry( CommandList*, const rtSurfaceInfo_t& ) {}
void GpuAccelerationStructure::BuildPendingGeometry( CommandList* ) {}
void GpuAccelerationStructure::UpdateSurfaceInstance( uint32_t, uint32_t, uint32_t, uint32_t, const mat4x4f& ) {}
void GpuAccelerationStructure::Update( CommandList* ) {}
void GpuAccelerationStructure::Destroy() {}

#endif
