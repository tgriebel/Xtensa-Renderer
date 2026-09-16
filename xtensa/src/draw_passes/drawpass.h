#pragma once

#include "../globals/common.h"
#include "../scene/camera.h"
#include "../asset_types/material.h"
#include "../render_binding/pipeline.h"
#include "../render_binding/shaderBinding.h"
#include "../render_resources/imageArray.h"
#include "../render_state/frameBuffer.h"

class ResourceContext;

struct scissor_t
{
	int32_t		x;
	int32_t		y;
	uint32_t	width;
	uint32_t	height;
};

class DrawPass
{
private:
	scissor_t			m_scissor;
	viewport_t			m_viewport;
	FrameBuffer*		m_fb;

protected:
	const char*			m_name;
	drawPass_t			m_passId;
	imageSamples_t		m_sampleRate;
	gfxStateBits_t		m_stateBits;


	void SetFrameBuffer( FrameBuffer* frameBuffer )
	{
		m_sampleRate = frameBuffer->SampleCount();
		m_viewport.x = 0;
		m_viewport.y = 0;
		m_viewport.width = frameBuffer->GetWidth();
		m_viewport.height = frameBuffer->GetHeight();

		m_scissor.x = m_viewport.x;
		m_scissor.y = m_viewport.y;
		m_scissor.width = m_viewport.width;
		m_scissor.height = m_viewport.height;

		m_fb = frameBuffer;
	}

public:
	virtual void Init( RenderContext* renderContext, FrameBuffer* fb ) = 0;

	virtual void FrameBegin( const ResourceContext* resources ) = 0;
	virtual void FrameEnd() = 0;

	inline void Resize()
	{
		SetFrameBuffer( m_fb );
	}

	inline drawPass_t Type() const
	{
		return m_passId;
	}

	inline const char* Name() const
	{
		return m_name;
	}

	inline gfxStateBits_t StateBits() const
	{
		return m_stateBits;
	}

	inline imageSamples_t SampleRate() const
	{
		return m_sampleRate;
	}

	inline void SetViewport( const int32_t x, const int32_t y, const uint32_t width, const uint32_t height )
	{
		m_viewport.x = x;
		m_viewport.y = y;
		m_viewport.width = width;
		m_viewport.height = height;
	}

	inline const viewport_t& GetViewport() const
	{
		return m_viewport;
	}

	inline void SetScissor( const int32_t x, const int32_t y, const uint32_t width, const uint32_t height )
	{
		m_scissor.x = x;
		m_scissor.y = y;
		m_scissor.width = width;
		m_scissor.height = height;
	}

	inline const scissor_t& GetScissor() const
	{
		return m_scissor;
	}

	inline const FrameBuffer* GetFrameBuffer() const
	{
		return m_fb;
	}

	inline FrameBuffer* GetFrameBuffer()
	{
		return m_fb;
	}

	inline bool NeedsMrt() const
	{
		return HasFlags( m_stateBits, GFX_STATE_MRT_ENABLE ) && ( GetFrameBuffer()->ColorLayerCount() > 1 );
	}

	void InsertResourceBarriers( CommandList& cmdContext );

	ImageArray					codeImages;
	ImageArray					codeCubeImages;
	ShaderBindParms*			parms;
};
