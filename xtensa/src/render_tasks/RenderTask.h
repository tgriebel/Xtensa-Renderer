#pragma once

#include <functional>

#include "../render_core/GpuSync.h"
#include "../render_state/frameBuffer.h"
#include "../render_resources/imageView.h"
#include "../asset_types/material.h"

class CommandList;
class GfxCmdList;
class RenderView;
class RenderContext;
class ResourceContext;
class Image;
class TaskSchedule;

using TaskCallback = std::function<void()>;

class GpuTask
{
protected:

	static const uint32_t MaxCustomPushConstantBytes = 128;
	static const uint32_t MaxCustomConstantBytes = 512;

	GpuTask*		m_child			= nullptr;
	bool			m_enabled		= true;
	TaskCallback	m_controlsFn	= nullptr;
	TaskCallback	m_frameBeginFn	= nullptr;

	uint32_t		m_shaderConstantsByteSize = 0;
	uint8_t			m_shaderConstants[ MaxCustomConstantBytes ];

public:
	virtual void			FrameBegin() {};
	virtual void			FrameEnd() {};
	virtual void			Resize() = 0;
	virtual void			Execute( CommandList& context ) = 0;
	virtual std::string		AsString() const = 0;
	const GpuTask*			GetChild() const { return m_child; };
	GpuTask*				GetChild() { return m_child; };

	bool					IsEnabled() const { return m_enabled; }
	void					SetEnabled( bool enabled ) { m_enabled = enabled; }

	void					RegisterControls( TaskCallback fn ) { m_controlsFn = std::move( fn ); }
	void					SetControls() const { if ( m_controlsFn ) { m_controlsFn(); } }
	bool					HasControls() const { return m_controlsFn != nullptr; }

	void					RegisterFrameBeginCallback( TaskCallback fn ) { m_frameBeginFn = std::move( fn ); }
	void					OnFrameBegin()  const{ if( m_frameBeginFn ) { m_frameBeginFn(); } }

	bool					HasConstants() const { return ( m_shaderConstantsByteSize > 0 ); }

	template<typename T>
	T*						GetConstants()
	{
		assert( HasConstants() );
		return reinterpret_cast<T*>( &m_shaderConstants );
	}

	template<typename T>
	T*						GetShaderConstants() { return reinterpret_cast<T*>( &m_shaderConstants ); }

	void SetChild( GpuTask* child )
	{
		if ( child == nullptr ) {
			return;
		}
		m_child = child;
	};

	virtual TaskSchedule*	GetSubSchedule() { return nullptr; }

	virtual ~GpuTask() {};
};



class RenderTask : public GpuTask
{
private:
	RenderView*		m_renderView;
	drawPass_t		m_beginPass;
	drawPass_t		m_endPass;
	GpuSemaphore	m_finishedSemaphore;
	FrameBuffer*	m_frameBuffer = nullptr;

	void Init( RenderView* view, drawPass_t begin, drawPass_t end );
	void Shutdown();
	void RenderViewSurfaces( GfxCmdList* context, const uint32_t multiViewIndex );

public:
	RenderTask()
	{
		Init( nullptr, DRAWPASS_COUNT, DRAWPASS_COUNT );
	}

	RenderTask( RenderView* view, drawPass_t begin, drawPass_t end )
	{
		Init( view, begin, end );
	}

	RenderTask( RenderView* view, drawPass_t begin, drawPass_t end, const frameBufferCreateInfo_t& fbInfo )
	{
		Init( view, begin, end );
		CreateFrameBuffer( fbInfo );
	}

	~RenderTask()
	{
		Shutdown();
	}

	void			CreateFrameBuffer( const frameBufferCreateInfo_t& fbInfo );

	void			FrameBegin();
	void			FrameEnd();
	void			Resize();
	std::string		AsString() const;

	void			Execute( CommandList& context ) override;
};
