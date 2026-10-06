#include "../asset_types/image.h"
#include "frameBuffer.h"
#include "deviceContext.h"
#include "../render_core/renderContext.h"

void FrameBuffer::Create( const frameBufferCreateInfo_t& createInfo )
{
	// Managed Resource
	{
		RenderResource::Create( resourceType_t::FRAMEBUFFER, createInfo.lifetime );
	}

	assert( createInfo.context != nullptr );

	if ( createInfo.context == nullptr ) {
		THROW_ERROR( "Framebuffer missing render context." );
	}

	m_createInfo = createInfo;

	if ( m_bufferCount > 0 ) {
		THROW_ERROR( "Framebuffer already initialized." );
	}

	m_bufferCount = ( createInfo.swapBuffering == swapBuffering_t::MULTI_FRAME ) ? MaxFrameStates : 1;

	m_colorCount += ( createInfo.color0 != nullptr ) ? 1 : 0;
	m_colorCount += ( createInfo.color1 != nullptr ) ? 1 : 0;
	m_colorCount += ( createInfo.color2 != nullptr ) ? 1 : 0;
	m_dsCount += ( createInfo.depthStencil != nullptr ) ? 1 : 0;
	m_dsCount += ( createInfo.stencil != nullptr ) ? 1 : 0;

	m_attachmentCount = m_colorCount + m_dsCount;

	Image* images[ MaxAttachmentCount ];
	images[ 0 ] = createInfo.color0;
	images[ 1 ] = createInfo.color1;
	images[ 2 ] = createInfo.color2;
	images[ 3 ] = createInfo.depthStencil;
	images[ 4 ] = createInfo.stencil;

	uint32_t firstValidIx = MaxAttachmentCount;
	for ( uint32_t imageIx = 0; imageIx < MaxAttachmentCount; ++imageIx ) {
		if ( images[ imageIx ] != nullptr ) {
			firstValidIx = imageIx;
			break;
		}
	}

	// Validation
	{
		if( firstValidIx == MaxAttachmentCount ) {
			THROW_ERROR( "No images provided." );
		}

		if ( ( createInfo.color0 == nullptr ) &&
			( ( createInfo.color1 != nullptr ) || ( createInfo.color2 != nullptr ) ) ) {
			THROW_ERROR( "Color attachment 0 has to be used if 1 and 2 are." );
		}

		if ( ( createInfo.color2 != nullptr ) && ( createInfo.color1 == nullptr ) ) {
			THROW_ERROR( "Color attachment 1 has to be used if 2 is." );
		}

		for ( uint32_t imageIx = firstValidIx + 1; imageIx < MaxAttachmentCount; ++imageIx )
		{
			if( images[ imageIx ] == nullptr ) {
				continue;
			}
			if( images[ firstValidIx ]->info.width != images[ imageIx ]->info.width ||
				images[ firstValidIx ]->info.height != images[ imageIx ]->info.height ||
				images[ firstValidIx ]->info.layers != images[ imageIx ]->info.layers )
			{
				THROW_ERROR( "Framebuffer images must have the same dimensions." );
			}
		}

		if( createInfo.lifetime == resourceLifeTime_t::RESIZE )
		{
			for ( uint32_t imageIx = 0; imageIx < MaxAttachmentCount; ++imageIx )
			{
				if( images[ imageIx ] == nullptr ) {
					continue;
				}
				if( HasFlags( images[ imageIx ]->gpuImage->GetFlags(), gpuImageStateFlags_t::GPU_IMAGE_PRESENT ) ) {
					continue;
				}
				if( images[ imageIx ]->GetLifetime() != resourceLifeTime_t::RESIZE )
				{
					THROW_ERROR( "Framebuffer images must also be RESIZE lifetime." );
				}
			}
		}
	}

	// Attachment bits
	m_attachmentBits = {};
	m_attachmentMask = RENDER_PASS_MASK_NONE;
	{
		if ( createInfo.color0 != nullptr )
		{
			m_attachmentBits.color0.samples = createInfo.color0->info.subsamples;
			m_attachmentBits.color0.fmt = createInfo.color0->info.fmt;
			m_attachmentMask |= RENDER_PASS_MASK_COLOR0;
		}
		if ( createInfo.color1 != nullptr )
		{
			m_attachmentBits.color1.samples = createInfo.color1->info.subsamples;
			m_attachmentBits.color1.fmt = createInfo.color1->info.fmt;
			m_attachmentMask |= RENDER_PASS_MASK_COLOR1;
		}
		if ( createInfo.color2 != nullptr )
		{
			m_attachmentBits.color2.samples = createInfo.color2->info.subsamples;
			m_attachmentBits.color2.fmt = createInfo.color2->info.fmt;
			m_attachmentMask |= RENDER_PASS_MASK_COLOR2;
		}

		if ( createInfo.depthStencil != nullptr )
		{
			m_attachmentBits.depth.samples = createInfo.depthStencil->info.subsamples;
			m_attachmentBits.depth.fmt = createInfo.depthStencil->info.fmt;
			m_attachmentMask |= RENDER_PASS_MASK_DEPTH;
		}

		if ( createInfo.stencil != nullptr )
		{
			m_attachmentBits.stencil.samples = createInfo.stencil->info.subsamples;
			m_attachmentBits.stencil.fmt = createInfo.stencil->info.fmt;
			m_attachmentMask |= RENDER_PASS_MASK_STENCIL;
		}
	}

	m_color0 = createInfo.color0;
	m_color1 = createInfo.color1;
	m_color2 = createInfo.color2;
	m_depthStencil = createInfo.depthStencil;
	m_stencil = createInfo.stencil;

	m_width = images[ firstValidIx ]->info.width;
	m_height = images[ firstValidIx ]->info.height;
	m_swapBuffering = createInfo.swapBuffering;
	
	if( m_color0 == nullptr ) {
		assert( ( m_color1 == nullptr ) && ( m_color2 == nullptr ) );
	}
}


void FrameBuffer::Destroy()
{
	m_colorCount = 0;
	m_dsCount = 0;
	m_attachmentCount = 0;
	m_bufferCount = 0;
}


bool FrameBuffer::NeedsResize() const
{
	return ( m_createInfo.context == nullptr ) || ( m_createInfo.context->FrameNumber() != m_lastResizeFrame );
}


bool FrameBuffer::OnResize( const uint32_t w, const uint32_t h )
{
	// TOOD: Remove?
	if( m_createInfo.lifetime != resourceLifeTime_t::RESIZE ) { // What if backing images are marked as resize?
		return false;
	}

	// TOOD: Remove?
	if( NeedsResize() == false ) {
		return false;
	}

	Destroy();
	Create( m_createInfo );

	m_lastResizeFrame = m_createInfo.context->FrameNumber();

	return true;
}