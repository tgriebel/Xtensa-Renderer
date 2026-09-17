#include "RayTracingTask.h"

#include "../render_resources/gpuBuffer.h"
#include "../render_binding/bindings.h"
#include "../render_binding/pipeline.h"
#include "../asset_types/gpuProgram.h"
#include "../asset_types/assetLib.h"
#include "../globals/assetDefs.h"
#include "../render_resources/gpuAccelerationStructure.h"


std::string RayTracingTask::AsString() const
{
	std::stringstream ss;
	ss << "RayTracingTask: " << m_name;
	return ss.str();
}


void RayTracingTask::Init( const rayTracingTaskCreateInfo_t& info )
{
	m_context = info.context;
	m_resources = info.resources;
	m_name = info.name;
	m_image = info.image;
	m_tlas = info.tlas;
	m_geometry = info.geometry;
	m_rtOutputImage = info.rtOutputImage;
	m_viewId = info.viewId;

	m_shaderConstantsByteSize = 0;
	if ( ( info.constants != nullptr ) && ( info.constantsByteSize > 0 ) )
	{
		m_shaderConstantsByteSize = Min( info.constantsByteSize, MaxUserConstantSizeInBytes );
		memcpy( m_shaderConstants, info.constants, m_shaderConstantsByteSize );
	}

	Asset<GpuProgram>* rgenAsset = GpuProgramLib().Find( info.rgenProgName );
	Asset<GpuProgram>* missAsset = GpuProgramLib().Find( info.missProgName );
	Asset<GpuProgram>* hitGroupAsset = GpuProgramLib().Find( info.hitGroupProgName );

	assert( rgenAsset && missAsset && hitGroupAsset );

#ifdef USE_VULKAN_RTX
	m_pipelineHdl = CreateRtPipeline( *rgenAsset, *missAsset, *hitGroupAsset );
#endif

	m_parms = m_context->RegisterBindParm( m_name, m_context->LookupBindSet( info.bindSetId ) );
}


void RayTracingTask::FrameBegin()
{
#ifdef USE_VULKAN_RTX
	if ( m_tlas != nullptr && m_tlas->IsBuilt() ) {
		m_parms->Bind( bind_tlas, ShaderAttachment( m_tlas ) );
	} else {
		m_parms->Unbind( bind_tlas );
	}
	if ( m_rtOutputImage != nullptr ) {
		m_parms->Bind( bind_rtOutputImage, ShaderAttachment( m_rtOutputImage ) );
	}
	if ( m_geometry != nullptr ) {
		m_parms->Bind( bind_rtVertexBuffer,  ShaderAttachment( &m_geometry->vb ) );
		m_parms->Bind( bind_rtIndexBuffer,   ShaderAttachment( &m_geometry->ib ) );
	}
	if ( m_tlas != nullptr && m_tlas->GetSurfaceInfoBuffer()->GetMaxSize() > 0 ) {
		m_parms->Bind( bind_rtSurfaceInfos, ShaderAttachment( m_tlas->GetSurfaceInfoBuffer() ) );
	}
	m_parms->Bind( bind_lightBuffer, ShaderAttachment( &m_resources->lightParms ) );
#endif
	GpuTask::OnFrameBegin();
}


void RayTracingTask::Execute( CommandList& cmdContext )
{
#ifdef USE_VULKAN_RTX
	cmdContext.MarkerBeginRegion( m_name.c_str(), ColorToVector( ColorLGrey ) );

	assert( m_image != nullptr );
	const uint32_t width  = m_image->info.width;
	const uint32_t height = m_image->info.height;

	// Build push-constant block: [baseConstants_t | user constants]
	uint8_t pushData[ MaxCustomPushConstantBytes ] = {};

	baseConstants_t& base = *reinterpret_cast<baseConstants_t*>( pushData );
	base.viewId = static_cast<uint32_t>( m_viewId );
	base.width  = width;
	base.height = height;

	if ( m_shaderConstantsByteSize > 0 ) {
		memcpy( pushData + ReservedConstantSizeInBytes, m_shaderConstants, m_shaderConstantsByteSize );
	}

	const uint32_t totalBytes = ReservedConstantSizeInBytes + m_shaderConstantsByteSize;
	cmdContext.TraceRays( m_pipelineHdl, *m_parms, pushData, totalBytes, width, height );

	cmdContext.MarkerEndRegion();
#endif
}


void RayTracingTask::Shutdown()
{
}
