#pragma once

#include "../asset_types/image.h"
#include "../globals/common.h"
#include "../render_state/rhi.h"
#include "../render_state/frameBuffer.h"
#include "../asset_types/gpuProgram.h"
#include "../render_resources/gpuBuffer.h"

class RenderContext;

enum gfxStateBits_t : uint64_t
{
	GFX_STATE_NONE				= 0,
	GFX_STATE_DEPTH_TEST		= ( 1 << 0 ),
	GFX_STATE_DEPTH_WRITE		= ( 1 << 1 ),
	GFX_STATE_COLOR0_MASK		= ( 1 << 2 ),
	GFX_STATE_COLOR1_MASK		= ( 1 << 3 ),
	GFX_STATE_COLOR2_MASK		= ( 1 << 4 ),
	GFX_STATE_DEPTH_OP_0		= ( 1 << 5 ),
	GFX_STATE_DEPTH_OP_1		= ( 1 << 6 ),
	GFX_STATE_DEPTH_OP_2		= ( 1 << 7 ),
	GFX_STATE_CULL_MODE_BACK	= ( 1 << 8 ),
	GFX_STATE_CULL_MODE_FRONT	= ( 1 << 9 ),
	GFX_STATE_WIREFRAME			= ( 1 << 10 ),
	GFX_STATE_CULL_BACKFACE		= ( 1 << 11 ),
	GFX_STATE_WIND_CLOCKWISE	= ( 1 << 12 ),
	GFX_STATE_BLEND_ENABLE		= ( 1 << 13 ),
	GFX_STATE_WIREFRAME_ENABLE	= ( 1 << 14 ),
	GFX_STATE_STENCIL_ENABLE	= ( 1 << 15 ),
	GFX_STATE_COLOR_ENABLE		= ( 1 << 16 ),
	GFX_STATE_MRT_ENABLE		= ( 1 << 17 ),
	GFX_STATE_BIT_17			= ( 1 << 18 ),
};
DEFINE_ENUM_OPERATORS( gfxStateBits_t, uint64_t )

struct rasterPipelineState_t
{
	gfxStateBits_t				stateBits;
	imageSamples_t				samplingRate;
	renderPassAttachmentMask_t	attachmentMask;
	renderAttachmentBits_t		passBits;
	float						viewportX;
	float						viewportY;
	float						viewportWidth;
	float						viewportHeight;
};


struct computePipelineState_t
{
};


#ifdef USE_VULKAN_RTX
struct vk_shaderBindTable_t
{
	VkStridedDeviceAddressRegionKHR	rgenRegion;
	VkStridedDeviceAddressRegionKHR	missRegion;
	VkStridedDeviceAddressRegionKHR	hitGroupRegion;
	VkStridedDeviceAddressRegionKHR	callableRegion;
};
#endif


struct rtPipelineState_t
{
#ifdef USE_VULKAN_RTX
	hdl_t					missProgHdl;
	hdl_t					hitGroupProgHdl;
	const GpuProgram*		missProg;
	const GpuProgram*		hitGroupProg;
	vk_shaderBindTable_t	sbt;
	GpuBuffer*				sbtBuffer;
#endif
};


struct pipelineState_t
{
	pipelineType_t				type;
	hdl_t						progHdl;
	shaderPermId_t				permSet;
	union
	{
		rasterPipelineState_t	rasterState;
		computePipelineState_t	computeState;
		rtPipelineState_t		rtState;
	};
	const GpuProgram*			prog;
	const char*					dbgProgName;

};


inline bool operator<( const pipelineState_t& a, const pipelineState_t& b )
{
	if( a.type != b.type )
	{
		return a.type < b.type;
	}

	if( a.progHdl != b.progHdl )
	{
		return a.progHdl < b.progHdl;
	}
	if( a.permSet != b.permSet )
	{
		return a.permSet < b.permSet;
	}

	assert( a.prog != nullptr );
	assert( a.prog == b.prog );

	// Both objects are known to have the same type/progHdl/permSet at this point.
	// For RASTER, the raster state bits further distinguish pipelines (one program
	// can produce multiple pipelines with different blend/depth/viewport configs).
	// For COMPUTE and RT, type+progHdl+permSet fully identify the pipeline.
	if( a.type == pipelineType_t::RASTER )
	{
		if( a.rasterState.stateBits != b.rasterState.stateBits )
		{
			return a.rasterState.stateBits < b.rasterState.stateBits;
		}
		if( a.rasterState.samplingRate != b.rasterState.samplingRate )
		{
			return a.rasterState.samplingRate < b.rasterState.samplingRate;
		}
		if( a.rasterState.viewportX != b.rasterState.viewportX )
		{
			return a.rasterState.viewportX < b.rasterState.viewportX;
		}
		if( a.rasterState.viewportY != b.rasterState.viewportY )
		{
			return a.rasterState.viewportY < b.rasterState.viewportY;
		}
		if( a.rasterState.viewportWidth != b.rasterState.viewportWidth )
		{
			return a.rasterState.viewportWidth < b.rasterState.viewportWidth;
		}
		if( a.rasterState.viewportHeight != b.rasterState.viewportHeight )
		{
			return a.rasterState.viewportHeight < b.rasterState.viewportHeight;
		}
		if( a.rasterState.attachmentMask != b.rasterState.attachmentMask )
		{
			return a.rasterState.attachmentMask < b.rasterState.attachmentMask;
		}
		return a.rasterState.passBits < b.rasterState.passBits;
	}
	return false;
}

class DrawPass;
class ShaderBindSet;


// TODO: Use the same resource model as Image, ImageView, GpuBuffer, etc
// Pipeline should init the actual API object and contain it
class Pipeline
{
private:
	const char*			m_dbgName = "";
	pipelineState_t		m_hashState = {};
	const DrawPass*		m_drawPass = nullptr;
	const GpuProgram*	m_prog = nullptr;
	shaderPermId_t		m_permSet = shaderPermId_t::NONE;

public:

	Pipeline( const DrawPass* pass, const Asset<GpuProgram>& progAsset, const shaderPermId_t permSet = shaderPermId_t::NONE );

	inline const pipelineState_t& GetHashState() const
	{
		return m_hashState;
	}

	inline const DrawPass* GetDrawPass() const
	{
		return m_drawPass;
	}

	inline const GpuProgram* GetGpuProgram() const
	{
		return m_prog;
	}

	inline const shaderPermId_t& GetShaderPerm() const
	{
		return m_permSet;
	}

	inline const char* GetDebugName() const
	{
		return m_dbgName;
	}
};


struct pipelineObject_t
{
	pipelineState_t		state;
#ifdef USE_VULKAN
	VkPipeline			pipeline		= VK_NULL_HANDLE;
	VkPipelineLayout	pipelineLayout	= VK_NULL_HANDLE;
#endif
	const GpuProgram*	prog			= nullptr;
	const char*			dbgProgName		= nullptr;
};


class ShaderBindSet;

void	ClearPipelineCache();
void	DestroyPipelineCache();
bool	GetPipelineObject( hdl_t hdl, pipelineObject_t** pipelineObject );
hdl_t	FindPipelineObject( const DrawPass* pass, const Asset<GpuProgram>& progAsset, const shaderPermId_t permSet );
hdl_t	CreateGraphicsPipeline( const DrawPass* pass, const Asset<GpuProgram>& prog, const shaderPermId_t permSet = shaderPermId_t::NONE );
hdl_t	CreateGraphicsPipeline( const hdl_t pipelineHdl, const pipelineState_t& state );
void	DestoryAllPipelines( const Asset<GpuProgram>& progAsset );
hdl_t	CreateComputePipeline( const Asset<GpuProgram>& prog );
hdl_t	CreateComputePipeline( const hdl_t pipelineHdl, const pipelineState_t& state );

#ifdef USE_VULKAN_RTX
hdl_t	CreateRtPipeline( const Asset<GpuProgram>& rgenProg, const Asset<GpuProgram>& missProg, const Asset<GpuProgram>& hitGroupProg );
hdl_t	CreateRtPipeline( hdl_t pipelineHdl, const pipelineState_t& state );
#endif