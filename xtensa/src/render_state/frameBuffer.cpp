#include "../asset_types/image.h"
#include "frameBuffer.h"
#include "deviceContext.h"
#include "rhi.h"
#include "../render_core/renderer.h"
#include "../render_core/swapChain.h"

extern SwapChain g_swapChain;

struct renderTransitionBits_t
{
	renderPassTransition_t			colorTrans0;
	renderPassTransition_t			colorTrans1;
	renderPassTransition_t			colorTrans2;
	renderPassTransition_t			depthTrans;
	renderPassTransition_t			stencilTrans;
};


struct vk_RenderPassBits_t
{
	union
	{
		struct vkRenderPassState_t
		{
			renderAttachmentBits_t		attachmentBits;
			renderTransitionBits_t		transitionBits;
			renderPassAttachmentMask_t	attachmentMask; // Mask for which attachments are used
		} semantic;
		uint8_t bytes[ VkPassBitsSize ];
	};

	vk_RenderPassBits_t()
	{
		memset( bytes, 0, VkPassBitsSize );
	}
};
static_assert( sizeof( vk_RenderPassBits_t ) == VkPassBitsSize, "Bits overflowed" );


struct renderPassTuple_t
{
	VkRenderPass		pass;
	vk_RenderPassBits_t	state;
};


VkRenderPass vk_CreateRenderPass( const vk_RenderPassBits_t& passState )
{
	VkRenderPass pass = VK_NULL_HANDLE;

	VkAttachmentReference colorAttachmentRef[ 3 ] = { };
	VkAttachmentReference dsAttachmentRef{ };

	uint32_t count = 0;
	uint32_t colorCount = 0;

	colorAttachmentRef[ 0 ].attachment = VK_ATTACHMENT_UNUSED;
	colorAttachmentRef[ 1 ].attachment = VK_ATTACHMENT_UNUSED;
	colorAttachmentRef[ 2 ].attachment = VK_ATTACHMENT_UNUSED;
	dsAttachmentRef.attachment = VK_ATTACHMENT_UNUSED;

	VkAttachmentDescription attachments[ 5 ] = {};

	if ( ( passState.semantic.attachmentMask & RENDER_PASS_MASK_COLOR0 ) != 0 )
	{
		if ( passState.semantic.transitionBits.colorTrans0.flags.presentAfter ) {
			attachments[ count ].format = vk_GetTextureFormat( g_swapChain.GetBackBufferFormat() );
		}
		else {
			attachments[ count ].format = vk_GetTextureFormat( passState.semantic.attachmentBits.color0.fmt );
		}
		attachments[ count ].samples = vk_GetSampleCount( passState.semantic.attachmentBits.color0.samples );
		attachments[ count ].loadOp = passState.semantic.transitionBits.colorTrans0.flags.clear ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
		attachments[ count ].storeOp = passState.semantic.transitionBits.colorTrans0.flags.store ? VK_ATTACHMENT_STORE_OP_STORE : VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[ count ].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[ count ].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

		if( passState.semantic.transitionBits.colorTrans0.flags.presentBefore ) {
			attachments[ count ].initialLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
		} else if ( passState.semantic.transitionBits.colorTrans0.flags.readBefore ) {
			attachments[ count ].initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		} else {
			attachments[ count ].initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		}

		if ( passState.semantic.transitionBits.colorTrans0.flags.presentAfter ) {
			attachments[ count ].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
		} else if ( passState.semantic.transitionBits.colorTrans0.flags.readAfter ) {
			attachments[ count ].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		} else {
			attachments[ count ].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		}

		colorAttachmentRef[ count ].attachment = count;
		colorAttachmentRef[ count ].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		++count;
	}

	if ( ( passState.semantic.attachmentMask & RENDER_PASS_MASK_COLOR1 ) != 0 )
	{
		attachments[ count ].format = vk_GetTextureFormat( passState.semantic.attachmentBits.color1.fmt );
		attachments[ count ].samples = vk_GetSampleCount( passState.semantic.attachmentBits.color1.samples );
		attachments[ count ].loadOp = passState.semantic.transitionBits.colorTrans1.flags.clear ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
		attachments[ count ].storeOp = passState.semantic.transitionBits.colorTrans1.flags.store ? VK_ATTACHMENT_STORE_OP_STORE : VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[ count ].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[ count ].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

		if ( passState.semantic.transitionBits.colorTrans1.flags.readBefore ) {
			attachments[ count ].initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		} else {
			attachments[ count ].initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		}

		if ( passState.semantic.transitionBits.colorTrans1.flags.readAfter ) {
			attachments[ count ].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		} else {
			attachments[ count ].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		}

		colorAttachmentRef[ count ].attachment = count;
		colorAttachmentRef[ count ].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		++count;
	}

	if ( ( passState.semantic.attachmentMask & RENDER_PASS_MASK_COLOR2 ) != 0 )
	{
		attachments[ count ].format = vk_GetTextureFormat( passState.semantic.attachmentBits.color2.fmt );
		attachments[ count ].samples = vk_GetSampleCount( passState.semantic.attachmentBits.color2.samples );
		attachments[ count ].loadOp = passState.semantic.transitionBits.colorTrans2.flags.clear ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
		attachments[ count ].storeOp = passState.semantic.transitionBits.colorTrans2.flags.store ? VK_ATTACHMENT_STORE_OP_STORE : VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[ count ].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[ count ].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

		if ( passState.semantic.transitionBits.colorTrans2.flags.readBefore ) {
			attachments[ count ].initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		} else {
			attachments[ count ].initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		}

		if ( passState.semantic.transitionBits.colorTrans2.flags.readAfter ) {
			attachments[ count ].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		} else {
			attachments[ count ].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		}

		colorAttachmentRef[ count ].attachment = count;
		colorAttachmentRef[ count ].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		++count;
	}

	colorCount = count;

	if ( ( passState.semantic.attachmentMask & RENDER_PASS_MASK_DEPTH ) != 0 )
	{
		attachments[ count ].format = vk_GetTextureFormat( passState.semantic.attachmentBits.depth.fmt );
		attachments[ count ].samples = vk_GetSampleCount( passState.semantic.attachmentBits.depth.samples );
		attachments[ count ].loadOp = passState.semantic.transitionBits.depthTrans.flags.clear ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
		attachments[ count ].storeOp = passState.semantic.transitionBits.depthTrans.flags.store ? VK_ATTACHMENT_STORE_OP_STORE : VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[ count ].stencilLoadOp = passState.semantic.transitionBits.depthTrans.flags.clear ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
		attachments[ count ].stencilStoreOp = passState.semantic.transitionBits.depthTrans.flags.store ? VK_ATTACHMENT_STORE_OP_STORE : VK_ATTACHMENT_STORE_OP_DONT_CARE;
		
		if ( passState.semantic.transitionBits.depthTrans.flags.readBefore ) {
			attachments[ count ].initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
		} else {
			attachments[ count ].initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		}

		if ( passState.semantic.transitionBits.depthTrans.flags.readAfter ) {
			attachments[ count ].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
		}
		else {
			attachments[ count ].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		}

		dsAttachmentRef.attachment = count;
		dsAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		++count;
	}

	if ( ( passState.semantic.attachmentMask & RENDER_PASS_MASK_STENCIL ) != 0 )
	{
		assert( ( passState.semantic.attachmentMask& RENDER_PASS_MASK_DEPTH ) != 0 );

		attachments[ count ].format = vk_GetTextureFormat( passState.semantic.attachmentBits.stencil.fmt );
		attachments[ count ].samples = vk_GetSampleCount( passState.semantic.attachmentBits.stencil.samples );
		attachments[ count ].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[ count ].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[ count ].stencilLoadOp = passState.semantic.transitionBits.stencilTrans.flags.clear ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
		attachments[ count ].stencilStoreOp = passState.semantic.transitionBits.stencilTrans.flags.store ? VK_ATTACHMENT_STORE_OP_STORE : VK_ATTACHMENT_STORE_OP_DONT_CARE;
		
		if ( passState.semantic.transitionBits.stencilTrans.flags.readBefore ) {
			attachments[ count ].initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
		} else {
			attachments[ count ].initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		}

		if ( passState.semantic.transitionBits.stencilTrans.flags.readAfter ) {
			attachments[ count ].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
		}
		else {
			attachments[ count ].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		}

		++count;
	}

	VkSubpassDescription subpass{ };
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = colorCount;
	subpass.pColorAttachments = colorAttachmentRef;
	subpass.pDepthStencilAttachment = &dsAttachmentRef;

	VkRenderPassCreateInfo renderPassInfo{ };
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassInfo.attachmentCount = count;
	renderPassInfo.pAttachments = attachments;
	renderPassInfo.subpassCount = 1;
	renderPassInfo.pSubpasses = &subpass;

	VK_CHECK_RESULT( vkCreateRenderPass( context.device, &renderPassInfo, nullptr, &pass ) );

	//vk_SetObjectName( (uint64_t)pass, VK_OBJECT_TYPE_RENDER_PASS, vk_BuildObjectName( "RenderPass", name ).c_str() );

	return pass;
}


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

	renderPassTransition_t perms[ PassPermCount ];
	for ( uint32_t i = 0; i < PassPermCount; ++i ) {
		perms[ i ].bits = i;
	}

	m_bufferCount = ( createInfo.swapBuffering == swapBuffering_t::MULTI_FRAME ) ? MaxFrameStates : 1;
	const bool canPresent = ( createInfo.color0 != nullptr ) && ( createInfo.color0->info.fmt == g_swapChain.GetBackBufferFormat() );

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

	// Initialization
	for( uint32_t permIx = 0; permIx < PassPermCount; ++permIx )
	{
		const renderPassTransition_t& state = perms[ permIx ];

		if( ( canPresent == false ) && state.flags.presentAfter ) {
			continue;
		}

		// Can specify clear options, etc. Assigned to cached render pass that matches
		vk_RenderPassBits_t passBits = {};
		passBits.semantic.attachmentBits = m_attachmentBits;

		passBits.semantic.transitionBits.colorTrans0 = state;
		passBits.semantic.transitionBits.colorTrans1 = state;
		passBits.semantic.transitionBits.colorTrans2 = state;
		passBits.semantic.transitionBits.depthTrans = state;
		passBits.semantic.transitionBits.stencilTrans = state;
		passBits.semantic.attachmentMask = m_attachmentMask;

		for ( uint32_t frameIx = 0; frameIx < m_bufferCount; ++frameIx )
		{
			VkImageView attachments[ MaxAttachmentCount ] = {};
			
			uint32_t currentAttachment = 0;
			if ( createInfo.color0 != nullptr ) {
				attachments[ currentAttachment++ ] = createInfo.color0->gpuImage->GetVkImageView( frameIx );
			}
			if ( createInfo.color1 != nullptr ) {
				attachments[ currentAttachment++ ] = createInfo.color1->gpuImage->GetVkImageView( frameIx );
			}
			if ( createInfo.color2 != nullptr ) {
				attachments[ currentAttachment++ ] = createInfo.color2->gpuImage->GetVkImageView( frameIx );
			}
			if ( createInfo.depthStencil != nullptr ) {
				attachments[ currentAttachment++ ] = createInfo.depthStencil->gpuImage->GetVkImageView( frameIx );
			}
			if ( createInfo.stencil != nullptr ) {
				attachments[ currentAttachment++ ] = createInfo.stencil->gpuImage->GetVkImageView( frameIx );
			}
			assert( currentAttachment == m_attachmentCount );

			// Dynamic rendering: no VkFramebuffer needed — image views are bound
			// directly via VkRenderingAttachmentInfo at vkCmdBeginRendering time.
			(void)attachments;
		}
		m_color0 = createInfo.color0;
		m_color1 = createInfo.color1;
		m_color2 = createInfo.color2;
		m_depthStencil = createInfo.depthStencil;
		m_stencil = createInfo.stencil;
	}
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