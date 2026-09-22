#pragma once

#include "../globals/common.h"
#include "../render_resources/gpuBuffer.h"
#include "../render_core/gpuImage.h"
#include "../render_core/renderResource.h"
#include "rhi.h"
#include "../asset_types/image.h"
#include "../render_resources/imageView.h"


struct scissor_t
{
	int32_t		x;
	int32_t		y;
	uint32_t	width;
	uint32_t	height;
};


struct frameBufferCreateInfo_t
{
	swapBuffering_t		swapBuffering;
	resourceLifeTime_t	lifetime;
	RenderContext*		context;
	const char*			name;
	Image*				color0;
	Image*				color1;
	Image*				color2;
	Image*				depthStencil;
	Image*				stencil;

	frameBufferCreateInfo_t() :
		swapBuffering( swapBuffering_t::SINGLE_FRAME ),
		lifetime( resourceLifeTime_t::RESIZE )
	{
		name = "";

		context = nullptr;

		color0 = nullptr;
		color1 = nullptr;
		color2 = nullptr;
		depthStencil = nullptr;
		stencil = nullptr;
	}
};


class FrameBuffer : public RenderResource
{
private:
	static const uint32_t MaxAttachmentCount = 5;

	frameBufferCreateInfo_t		m_createInfo;

	Image*						m_color0;
	Image*						m_color1;
	Image*						m_color2;
	Image*						m_depthStencil;
	Image*						m_stencil;

	uint32_t					m_width;
	uint32_t					m_height;
	uint32_t					m_colorCount;
	uint32_t					m_dsCount;
	uint32_t					m_attachmentCount;
	uint32_t					m_bufferCount;
	uint64_t					m_lastResizeFrame;
	bool						m_isBackBuffer;

	swapBuffering_t				m_swapBuffering;
	renderPassAttachmentMask_t	m_attachmentMask;
	renderAttachmentBits_t		m_attachmentBits;

	inline uint32_t GetBufferId( const uint32_t bufferId = 0 ) const
	{
		const uint32_t bufferCount = ( m_swapBuffering == swapBuffering_t::MULTI_FRAME ) ? MaxFrameStates : 1;
		return Min( bufferId, bufferCount - 1 );
	}

public:

	FrameBuffer() :
		m_attachmentCount( 0 ),
		m_bufferCount( 0 ),
		m_colorCount( 0 ),
		m_dsCount( 0 ),
		m_width( 0 ),
		m_height( 0 ),
		m_isBackBuffer( false )
	{
		m_color0 = nullptr;
		m_color1 = nullptr;
		m_color2 = nullptr;
		m_depthStencil = nullptr;
		m_stencil = nullptr;
	}

	inline uint32_t GetWidth() const
	{
		return m_width;
	}

	inline uint32_t GetHeight() const
	{
		return m_height;
	}

	inline uint32_t ColorLayerCount() const
	{
		return m_colorCount;
	}

	inline uint32_t DepthLayerCount() const
	{
		return m_dsCount;
	}

	inline uint32_t LayerCount() const
	{
		return m_attachmentCount;
	}

	inline imageSamples_t SampleCount() const
	{
		return ( ColorLayerCount() > 0 ) ? GetColor()->info.subsamples : GetDepthStencil()->info.subsamples;
	}

	inline bool IsBackbuffer() const
	{
		if( m_color0 == nullptr ) {
			return false;
		}
		return HasFlags( m_color0->gpuImage->GetFlags(), gpuImageStateFlags_t::GPU_IMAGE_PRESENT );
	}

	inline const Image* GetColor() const
	{
		return ( m_colorCount > 0 ) ? m_color0 : nullptr;
	}

	inline const Image* GetColor1() const
	{
		return ( m_colorCount > 1 ) ? m_color1 : nullptr;
	}

	inline const Image* GetColor2() const
	{
		return ( m_colorCount > 2 ) ? m_color2 : nullptr;
	}

	inline const Image* GetDepthStencil() const
	{
		return ( m_dsCount >= 1 ) ? m_depthStencil : nullptr;
	}

	inline const Image* GetStencil() const
	{
		return ( m_dsCount >= 1 ) ? m_stencil : nullptr;
	}

	inline renderAttachmentBits_t GetAttachmentBits() const
	{
		return m_attachmentBits;
	}

	inline renderPassAttachmentMask_t GetAttachmentMask() const
	{
		return m_attachmentMask;
	}

	bool NeedsResize() const;

	void Create( const frameBufferCreateInfo_t& createInfo );
	void Destroy() override;
	bool OnResize( const uint32_t w, const uint32_t h ) override;
};
