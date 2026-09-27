#include "../../stdafx.h"

#include "pipeline.h"
#include "../render_core/renderer.h"
#include "../render_state/deviceContext.h"
#include "../render_state/rhi.h"
#include "../render_binding/bindings.h"
#include "shaderBinding.h"
#include "../scene/sceneBase.h"
#include "../asset_types/assetLib.h"
#include "../asset_types/binaryFile.h"
#include "vertexInput.h"

#include <SysCore/common.h>

static std::unordered_map< uint64_t, pipelineObject_t > s_runtimePipelineCache;
static std::unordered_map< uint64_t, std::set<pipelineState_t> > s_progToPipelines;


void CreatePipelineCache( const BinaryFile& cacheFile )
{
	if ( context.pipelineCache != VK_NULL_HANDLE ) {
		return;
	}
	context.pipelineCache = vk_CreatePipelineCache( cacheFile, "PipelineCache" );
}


void DumpPipelineCacheToDisk( const Asset<BinaryFile>& binaryFileAsset )
{
	if ( context.pipelineCache == VK_NULL_HANDLE ) {
		return;
	}

	const BinaryFile cacheData = vk_GetPipelineCacheData( context.pipelineCache );
	WriteBinaryFileToDisk( binaryFileAsset.Get().GetPath(), cacheData );

	vkDestroyPipelineCache( context.device, context.pipelineCache, nullptr );
	context.pipelineCache = VK_NULL_HANDLE;
}


void ClearRuntimePipelineCache()
{
	s_runtimePipelineCache.clear();
}


void DestroyRuntimePipelineCache()
{
	for ( auto it = s_runtimePipelineCache.begin(); it != s_runtimePipelineCache.end(); ++it )
	{
		pipelineObject_t& obj = it->second;
		vkDestroyPipeline( context.device, obj.pipeline, nullptr );
		vkDestroyPipelineLayout( context.device, obj.pipelineLayout, nullptr );
#ifdef USE_VULKAN_RTX
		const pipelineType_t type = obj.state.type;
		if ( type == pipelineType_t::RAY_TRACING )
		{
			rtPipelineState_t& rtState = obj.state.rtState;

			rtState.sbtBuffer->Destroy();
			delete rtState.sbtBuffer;
			rtState.sbtBuffer = nullptr;
		}
#endif
	}
	s_runtimePipelineCache.clear();
}


static hdl_t GetPipelineStateHash( const pipelineState_t& state )
{
	return Hash( reinterpret_cast<const uint8_t*>( &state ), sizeof( pipelineState_t ) );
}


pipelineState_t CreateGfxState( const DrawPass* pass, const Asset<GpuProgram>& progAsset, const shaderPermId_t permSet )
{
	pipelineState_t state {};
	state.type = pipelineType_t::RASTER;
	state.progHdl = progAsset.Handle();
	state.permSet = permSet;
	state.prog = &progAsset.Get();
	state.dbgProgName = progAsset.GetName().c_str();

	rasterPipelineState_t& rasterState = state.rasterState;

	const viewport_t& viewport = pass->GetViewport();

	rasterState.stateBits = pass->StateBits();
	rasterState.samplingRate = pass->SampleRate();
	rasterState.attachmentMask = pass->GetFrameBuffer()->GetAttachmentMask();
	rasterState.passBits = pass->GetFrameBuffer()->GetAttachmentBits();
	rasterState.viewportX = static_cast<float>( viewport.x );
	rasterState.viewportY = static_cast<float>( viewport.y );
	rasterState.viewportWidth = static_cast<float>( viewport.width );
	rasterState.viewportHeight = static_cast<float>( viewport.height );

	if( pass->NeedsMrt() ) {
		SetFlags( state.permSet, shaderPermId_t::MRT );
	} else {
		ClearFlags( state.permSet, shaderPermId_t::MRT );
	}

	return state;
}


pipelineState_t CreateComputeState( const Asset<GpuProgram>& progAsset )
{
	pipelineState_t state{};
	state.type = pipelineType_t::COMPUTE;
	state.progHdl = progAsset.Handle();
	state.prog = &progAsset.Get();
	state.dbgProgName = progAsset.GetName().c_str();

	return state;
}


bool GetPipelineObject( hdl_t hdl, pipelineObject_t** pipelineObject )
{
	auto it = s_runtimePipelineCache.find( hdl.Get() );
	if ( it != s_runtimePipelineCache.end() ) {
		*pipelineObject = &it->second;
		return true;
	}
	*pipelineObject = nullptr;
	return false;
}


hdl_t FindPipelineObject( const DrawPass* pass, const Asset<GpuProgram>& progAsset, const shaderPermId_t permSet )
{
	const pipelineState_t state = CreateGfxState( pass, progAsset, permSet );

	const hdl_t pipelineHdl = GetPipelineStateHash( state );

	auto it = s_runtimePipelineCache.find( pipelineHdl.Get() );
	if ( it != s_runtimePipelineCache.end() ) {
		return pipelineHdl;
	}
	return INVALID_HDL;
}


void DestoryAllPipelines( const Asset<GpuProgram>& progAsset )
{
	auto pipelineSetIt = s_progToPipelines.find( progAsset.Handle().Get() );

	if( pipelineSetIt == s_progToPipelines.end() ) {
		return;
	}

	std::set<pipelineState_t>& pipelineHandles = pipelineSetIt->second;

	for( auto pipelineState : pipelineHandles )
	{
		const hdl_t pipelineHdl = GetPipelineStateHash( pipelineState );

		auto pipelineIt = s_runtimePipelineCache.find( pipelineHdl.Get() );
		if( pipelineIt == s_runtimePipelineCache.end() ){
			continue;
		}
		vkDestroyPipeline( context.device, pipelineIt->second.pipeline, nullptr );
		vkDestroyPipelineLayout( context.device, pipelineIt->second.pipelineLayout, nullptr );

		pipelineIt->second.pipeline = VK_NULL_HANDLE;
		pipelineIt->second.pipelineLayout = VK_NULL_HANDLE;
	}
	pipelineHandles.clear();
}


hdl_t CreateGraphicsPipeline( const DrawPass* pass, const Asset<GpuProgram>& progAsset, const shaderPermId_t permSet )
{
	const pipelineState_t state = CreateGfxState( pass, progAsset, permSet );
	const hdl_t pipelineHdl = GetPipelineStateHash( state );
	return CreateGraphicsPipeline( pipelineHdl, state );
}


hdl_t CreateGraphicsPipeline( const hdl_t pipelineHdl, const pipelineState_t& state )
{
	auto it = s_runtimePipelineCache.find( pipelineHdl.Get() );
	const bool found = ( it != s_runtimePipelineCache.end() );

	pipelineObject_t pipelineObject{};

	if( found )
	{
		pipelineObject = it->second;
		if( pipelineObject.pipeline != VK_NULL_HANDLE ) {
			return pipelineHdl;
		}
	}
	else
	{
		pipelineObject.state = state;
		pipelineObject.prog = state.prog;
		pipelineObject.dbgProgName = state.dbgProgName;
	}

	assert( pipelineObject.prog == state.prog );
	assert( pipelineHdl != INVALID_HDL );

	if( !vk_CreateGraphicsPipeline( state, pipelineObject ) ) {
		return INVALID_HDL;
	}

	s_runtimePipelineCache[ pipelineHdl.Get() ] = pipelineObject;

	s_progToPipelines[ state.progHdl.Get()].insert(state);

	return pipelineHdl;
}


hdl_t CreateComputePipeline( const Asset<GpuProgram>& progAsset )
{
	const pipelineState_t state = CreateComputeState( progAsset );
	const hdl_t pipelineHdl = GetPipelineStateHash( state );
	return CreateComputePipeline( pipelineHdl, state );
}


hdl_t CreateComputePipeline( const hdl_t pipelineHdl, const pipelineState_t& state )
{
	auto it = s_runtimePipelineCache.find( pipelineHdl.Get() );
	const bool found = ( it != s_runtimePipelineCache.end() );

	pipelineObject_t pipelineObject{};

	if( found )
	{
		pipelineObject = it->second;
		if( pipelineObject.pipeline != VK_NULL_HANDLE )
		{
			return pipelineHdl;
		}
	}
	else
	{
		pipelineObject.state = state;
		pipelineObject.prog = state.prog;
		pipelineObject.dbgProgName = state.dbgProgName;
	}

#ifdef USE_VULKAN
	vk_CreateComputePipeline( state, pipelineObject );
#endif

	s_runtimePipelineCache[ pipelineHdl.Get() ] = pipelineObject;

	s_progToPipelines[ state.progHdl.Get() ].insert( state );

	return pipelineHdl.Get();
}


#ifdef USE_VULKAN_RTX



static pipelineState_t CreateRtState( const Asset<GpuProgram>& rgenProg, const Asset<GpuProgram>& missProg, const Asset<GpuProgram>& hitGroupProg )
{
	pipelineState_t state{};
	state.type = pipelineType_t::RAY_TRACING;
	state.progHdl = rgenProg.Handle();
	state.prog = &rgenProg.Get();
	state.dbgProgName = rgenProg.GetName().c_str();

	state.rtState.missProgHdl = missProg.Handle();
	state.rtState.hitGroupProgHdl = hitGroupProg.Handle();
	state.rtState.missProg = &missProg.Get();
	state.rtState.hitGroupProg = &hitGroupProg.Get();

	return state;
}


hdl_t CreateRtPipeline( const Asset<GpuProgram>& rgenProg, const Asset<GpuProgram>& missProg, const Asset<GpuProgram>& hitGroupProg )
{
	const pipelineState_t state = CreateRtState( rgenProg, missProg, hitGroupProg );
	const hdl_t pipelineHdl = GetPipelineStateHash( state );
	return CreateRtPipeline( pipelineHdl, state );
}


hdl_t CreateRtPipeline( const hdl_t pipelineHdl, const pipelineState_t& state )
{
	auto it = s_runtimePipelineCache.find( pipelineHdl.Get() );
	if( it != s_runtimePipelineCache.end() && it->second.pipeline != VK_NULL_HANDLE )
	{
		return pipelineHdl;
	}

	pipelineObject_t obj{};
	obj.state = state;
	obj.prog = state.prog;
	obj.dbgProgName = state.dbgProgName;

	vk_CreateRtPipeline( state, obj );

	s_runtimePipelineCache[ pipelineHdl.Get() ] = std::move( obj );

	return pipelineHdl;
}
#endif