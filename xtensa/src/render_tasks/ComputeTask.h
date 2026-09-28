#pragma once

#include <functional>

#include "../render_core/renderer.h"
#include "../render_resources/imageView.h"
#include "../render_resources/gpuBuffer.h"

class ShaderBindParms;
class ComputeTask;

struct computeTaskCreateInfo_t
{
	const char*			name;
	const char*			progName;
	RenderContext*		context;
	ResourceContext*	resources;
	const Image*		image;				// Optional. Used when `imageTileSize` is not 0

	uint64_t			bindSetId;			// Hash id of the target bindset (e.g. bindset_computeImageReadback)

	const void*			constants;			// Optional: custom shader constants
	uint32_t			constantsByteSize;	// Size in bytes

	uint32_t			imageTileSizeX;		// Optional: Used for processing over an image in tiles. Size is in pixels
	uint32_t			imageTileSizeY;		// Optional: Used for processing over an image in tiles. Size is in pixels

	uint32_t			dispatchX;			// Explicit dispatch controls. Used when imageTileSize is 0.
	uint32_t			dispatchY;			// Explicit dispatch controls. Used when imageTileSize is 0.
	uint32_t			dispatchZ;			// Explicit dispatch controls. Used when imageTileSize is 0.

	// Invoked once per frame to populate the bindset. The caller is responsible
	// for matching the slot names / resource types declared by bindSetId.
	std::function<void( ComputeTask* task, ShaderBindParms* )> bind;

	const void*			pushConstants;		// Optional. Set at init time
	uint32_t			pushConstantsSize;	// Optional
};


class ComputeTask : public GpuTask
{
private:
	RenderContext*			m_context;
	ResourceContext*		m_resources;
	ShaderBindParms*		m_parms;

	std::string				m_name;

	const Image*			m_image = nullptr;
	
	Asset<GpuProgram>*		m_progAsset;
	bool					m_dispatchByTile = false;
	uint32_t				m_imageTileSizeX;
	uint32_t				m_imageTileSizeY;
	uint32_t				m_dispatchX;
	uint32_t				m_dispatchY;
	uint32_t				m_dispatchZ;

	std::function<void( ComputeTask* task, ShaderBindParms* )> m_bind;

	void Init( const computeTaskCreateInfo_t& info );
	void Shutdown();

public:

	ComputeTask( const computeTaskCreateInfo_t& info )
	{
		Init( info );
	}

	~ComputeTask()
	{
		Shutdown();
	}

	void			Resize() {}
	void			FrameBegin();
	std::string		AsString() const;

	void Execute( CommandList& context ) override;
};
