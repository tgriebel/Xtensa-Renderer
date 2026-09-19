#pragma once

#include <functional>

#include "../render_core/renderer.h"
#include "../render_core/renderUploader.h"
#include "../render_resources/imageView.h"
#include "../render_resources/imageArray.h"
#include "../render_resources/gpuAccelerationStructure.h"

class ShaderBindParms;
class RayTracingTask;

static const uint32_t MaxRtCodeImages = 4;

struct rayTracingTaskCreateInfo_t
{
	const char*					name;
	const char*					rgenProgName;
	const char*					missProgName;
	const char*					hitGroupProgName;
	RenderContext*				context;
	ResourceContext*			resources;

	uint64_t					bindSetId;			// Bindset id
	uint64_t					viewId;				// View id

	const GpuAccelerationStructure* tlas;			// Scene TLAS
	const GeometryContext*		geometry;			// Needed for VB/IB
	const Image*				rtOutputImage;		// Storage image written by the rgen shader
	const Image*				codeImages[ MaxRtCodeImages ] = {};	// Optional. Input images for shader that are independent from global texture pool (static for a given frame)

	const void*					constants;			// Optional, Custom shader constants pushed at execute time
	uint32_t					constantsByteSize;	// Size in bytes
};


class RayTracingTask : public GpuTask
{
public:
	struct baseConstants_t
	{
		uint32_t viewId;
		uint32_t width;
		uint32_t height;
		uint32_t pad;
	};

	static const uint32_t ReservedConstantSizeInBytes = sizeof( baseConstants_t );
	static const uint32_t MaxUserConstantSizeInBytes  = MaxCustomPushConstantBytes - ReservedConstantSizeInBytes;

private:
	RenderContext*					m_context;
	ResourceContext*				m_resources;
	ShaderBindParms*				m_parms;

	std::string						m_name;

	hdl_t							m_pipelineHdl;
	uint64_t						m_viewId;

	const GpuAccelerationStructure*	m_tlas = nullptr;
	const GeometryContext*			m_geometry = nullptr;
	const Image*					m_rtOutputImage = nullptr;
	ImageArray						m_codeImages;

	void Init( const rayTracingTaskCreateInfo_t& info );
	void Shutdown();

public:

	RayTracingTask( const rayTracingTaskCreateInfo_t& info )
	{
		Init( info );
	}

	~RayTracingTask()
	{
		Shutdown();
	}

	void		Resize() {}
	void		FrameBegin();
	std::string	AsString() const;

	void Execute( CommandList& context ) override;
};
