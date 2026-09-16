#include "RenderTask.h"
#include "../render_core/renderer.h"
#include "../render_state/cmdContext.h"
#include "../render_core/renderview.h"
#include "../render_resources/gpuBuffer.h"
#include "../render_binding/bindings.h"
#include "../globals/assetDefs.h"

#if defined( USE_IMGUI )
#include "../../../external/imgui/imgui.h"
#include "../../../external/imgui/backends/imgui_impl_glfw.h"
#include "../../../external/imgui/backends/imgui_impl_vulkan.h"

#include "../app/imguiInterface.h"

extern imguiControls_t g_imguiControls;
#endif

static inline bool SkipPass( const drawSurf_t& surf, const drawPass_t pass )
{
	if ( surf.pipelineObject == INVALID_HDL ) {
		return true;
	}

	if ( ( surf.flags & SKIP_OPAQUE ) != 0 )
	{
		if ( ( pass == DRAWPASS_SHADOW ) ||
			( pass == DRAWPASS_DEPTH ) ||
			( pass == DRAWPASS_TERRAIN ) ||
			( pass == DRAWPASS_OPAQUE ) ||
			( pass == DRAWPASS_SKYBOX ) ||
			( pass == DRAWPASS_DEBUG_3D )
			) {
			return true;
		}
	}

	if ( ( pass == DRAWPASS_DEBUG_3D ) && ( ( surf.flags & DEBUG_SOLID ) == 0 ) ) {
		return true;
	}

	if ( ( pass == DRAWPASS_DEBUG_WIREFRAME ) && ( ( surf.flags & WIREFRAME ) == 0 ) ) {
		return true;
	}

	return false;
}


void RenderTask::RenderViewSurfaces( GfxCmdList* cmdContext, const uint32_t multiViewIndex )
{
	// For now the pass state is the same for the entire view region
	const DrawPass* pass = m_renderView->passes[ multiViewIndex ][ m_beginPass ];
	if ( pass == nullptr ) {
		THROW_ERROR( "Missing pass state!" );
	}

	const FrameBuffer* fb = pass->GetFrameBuffer();

	const bool startOnFirstPass = ( m_beginPass == m_renderView->ViewRegionPassBegin() );
	const bool endOnLastPass = ( m_endPass == m_renderView->ViewRegionPassEnd() );
	const bool isBackBuffer = fb->IsBackbuffer();

	renderPassTransition_t transitionState = m_renderView->TransitionState();

	transitionState.flags.clear = m_renderView->Clear() && startOnFirstPass;
	transitionState.flags.store = true;

	// If this is the first write to the backbuffer then it needs to transition from the present state
	if( transitionState.flags.clear ) {
		transitionState.flags.presentBefore = isBackBuffer;
	}
	if( endOnLastPass && m_renderView->Finalize() ) {
		transitionState.flags.presentAfter = isBackBuffer;
	}

	const uint32_t colorAttachmentsCount = fb->ColorLayerCount();

	const VkAttachmentLoadOp  loadOp  = transitionState.flags.clear ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
	const VkAttachmentStoreOp storeOp = transitionState.flags.store ? VK_ATTACHMENT_STORE_OP_STORE : VK_ATTACHMENT_STORE_OP_DONT_CARE;

	VkClearValue clearColor = {};
	VkClearValue clearDepth = {};
	if ( transitionState.flags.clear )
	{
		const vec4f c = m_renderView->ClearColor();
		clearColor.color = { c[ 0 ], c[ 1 ], c[ 2 ], c[ 3 ] };
		clearDepth.depthStencil = { m_renderView->ClearDepth(), m_renderView->ClearStencil() };
	}

	const Image* colorImages[ 3 ] = { fb->GetColor(), fb->GetColor1(), fb->GetColor2() };
	VkRenderingAttachmentInfo colorAttachments[ 3 ] = {};
	for ( uint32_t i = 0; i < colorAttachmentsCount; ++i )
	{
		colorAttachments[ i ].sType       = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
		colorAttachments[ i ].imageView   = colorImages[ i ]->gpuImage->GetVkImageView( context.bufferId );
		colorAttachments[ i ].imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		colorAttachments[ i ].loadOp      = loadOp;
		colorAttachments[ i ].storeOp     = storeOp;
		colorAttachments[ i ].clearValue  = clearColor;
	}

	VkRenderingAttachmentInfo depthAttachment = {};
	const bool hasDepth = ( fb->GetDepthStencil() != nullptr );
	if ( hasDepth )
	{
		depthAttachment.sType       = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
		depthAttachment.imageView   = fb->GetDepthStencil()->gpuImage->GetVkImageView( context.bufferId );
		depthAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		depthAttachment.loadOp      = loadOp;
		depthAttachment.storeOp     = storeOp;
		depthAttachment.clearValue  = clearDepth;
	}

	VkRenderingAttachmentInfo stencilAttachment = {};
	const bool hasStencil = ( fb->GetStencil() != nullptr );
	if ( hasStencil )
	{
		stencilAttachment.sType       = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
		stencilAttachment.imageView   = fb->GetDepthStencil()->gpuImage->GetVkImageView( context.bufferId );
		stencilAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		stencilAttachment.loadOp      = loadOp;
		stencilAttachment.storeOp     = storeOp;
		stencilAttachment.clearValue  = clearDepth;
	}

	VkRenderingInfo renderingInfo = {};
	renderingInfo.sType                = VK_STRUCTURE_TYPE_RENDERING_INFO;
	renderingInfo.renderArea.offset    = { pass->GetViewport().x, pass->GetViewport().y };
	renderingInfo.renderArea.extent    = { pass->GetViewport().width, pass->GetViewport().height };
	renderingInfo.layerCount           = 1;
	renderingInfo.colorAttachmentCount = colorAttachmentsCount;
	renderingInfo.pColorAttachments    = colorAttachmentsCount > 0 ? colorAttachments : nullptr;
	renderingInfo.pDepthAttachment     = hasDepth    ? &depthAttachment   : nullptr;
	renderingInfo.pStencilAttachment   = hasStencil  ? &stencilAttachment : nullptr;

	VkCommandBuffer cmdBuffer = cmdContext->CommandBuffer();

	for ( uint32_t passIx = m_beginPass; passIx <= m_endPass; ++passIx )
	{
		DrawPass* pass = m_renderView->passes[ multiViewIndex ][ passIx ];
		if ( pass == nullptr ) {
			continue;
		}
		// These barriers could be tighter by places them in between passes
		pass->InsertResourceBarriers( *cmdContext );
	}

	if( transitionState.flags.readBefore || transitionState.flags.presentBefore )
	{
		const gpuImageStateFlags_t colorPriorState = transitionState.flags.presentBefore ? GPU_IMAGE_PRESENT : GPU_IMAGE_READ;
		for( uint32_t i = 0; i < colorAttachmentsCount; ++i )
		{
			Transition( cmdContext, *colorImages[ i ], colorPriorState, GPU_IMAGE_WRITE );
		}
	}

	if( hasDepth && transitionState.flags.readBefore ) {
		Transition( cmdContext, *fb->GetDepthStencil(), GPU_IMAGE_READ, GPU_IMAGE_WRITE );
	}

	vkCmdBeginRendering( cmdBuffer, &renderingInfo );

	uint32_t modelOffset = 0;

	for ( uint32_t passIx = m_beginPass; passIx <= m_endPass; ++passIx )
	{
		DrawPass* pass = m_renderView->passes[ multiViewIndex ][ passIx ];
		if ( pass == nullptr ) {
			continue;
		}

		const DrawGroup* drawGroup = &m_renderView->drawGroup[ passIx ];

		const uint32_t surfaceCount = drawGroup->Count();
		if( surfaceCount == 0 ) {
			continue;
		}

		const GeometryContext* geo = drawGroup->Geometry();

		cmdContext->MarkerBeginRegion( pass->Name(), ColorToVector( Color::Gold ) );

		VkBuffer vertexBuffers[] = { geo->vb.GetVkObject() };
		VkDeviceSize offsets[] = { 0 };
		vkCmdBindVertexBuffers( cmdContext->CommandBuffer(), 0, 1, vertexBuffers, offsets );
		vkCmdBindIndexBuffer( cmdContext->CommandBuffer(), geo->ib.GetVkObject(), 0, VK_INDEX_TYPE_UINT32 );

		const viewport_t& viewport = pass->GetViewport();

		VkViewport vk_viewport{ };
		vk_viewport.x = static_cast<float>( viewport.x );
		vk_viewport.y = static_cast<float>( viewport.y );
		vk_viewport.width = static_cast<float>( viewport.width );
		vk_viewport.height = static_cast<float>( viewport.height );
		vk_viewport.minDepth = 0.0f;
		vk_viewport.maxDepth = 1.0f;
		vkCmdSetViewport( cmdBuffer, 0, 1, &vk_viewport );

		VkRect2D rect{ };
		rect.extent.width = viewport.width;
		rect.extent.height = viewport.height;
		vkCmdSetScissor( cmdBuffer, 0, 1, &rect );
		vkCmdSetLineWidth( cmdBuffer, 1.0f );

		sortKey_t lastKey = {};
		lastKey.materialId = INVALID_HDL.Get();
		lastKey.stencilBit = 0;

		hdl_t pipelineHandle = INVALID_HDL;
		pipelineObject_t* pipelineObject = nullptr;

		for ( uint32_t surfIx = 0; surfIx < surfaceCount; ++surfIx )
		{
			const drawSurf_t& surface = drawGroup->DrawSurf( surfIx );
			const surfaceUpload_t& upload = drawGroup->SurfUpload( surfIx );

			if ( SkipPass( surface, drawPass_t( passIx ) ) ) {
				continue;
			}

			if ( lastKey.key != surface.sortKey.key )
			{
				cmdContext->MarkerInsert( surface.dbgName, ColorToVector( Color::LGrey ) );

				if ( passIx == DRAWPASS_DEPTH ) {
					vkCmdSetStencilReference( cmdBuffer, VK_STENCIL_FACE_FRONT_BIT, surface.stencilBit );
				}

				if( surface.pipelineObject != pipelineHandle )
				{
					GetPipelineObject( surface.pipelineObject, &pipelineObject );
					assert( pipelineObject != nullptr );

					// Rebuild the pipeline (should only happen on hot reloads)
					if ( pipelineObject->pipeline == VK_NULL_HANDLE ){	
						CreateGraphicsPipeline( surface.pipelineObject, pipelineObject->state );
					}
					assert( pipelineObject != nullptr );
					assert( pipelineObject->pipeline != nullptr );
					assert( pipelineObject->pipelineLayout != nullptr );

					const RenderContext* renderContext = cmdContext->GetRenderContext();

					const uint32_t descSetCount = 3;
					VkDescriptorSet descSetArray[ descSetCount ] = { renderContext->globalParms->GetVkObject(), m_renderView->BindParms()->GetVkObject(), pass->parms->GetVkObject() };

					vkCmdBindPipeline( cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineObject->pipeline );
					vkCmdBindDescriptorSets( cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineObject->pipelineLayout, 0, descSetCount, descSetArray, 0, nullptr );
					pipelineHandle = surface.pipelineObject;
				}
				lastKey = surface.sortKey;
			}

			assert( surface.sortKey.materialId < ( 1ull << KeyMaterialBits ) );

			gpuPushConstants_t pushConstants = {};
			pushConstants.viewId = uint32_t( m_renderView->GetViewBufferUploadId( multiViewIndex ) );
			pushConstants.objectId = surface.objectOffset + m_renderView->drawGroupOffset[ passIx ];
			pushConstants.materialId = uint32_t( surface.sortKey.materialId );

			vkCmdPushConstants( cmdBuffer, pipelineObject->pipelineLayout, VK_SHADER_STAGE_ALL, 0, sizeof( gpuPushConstants_t ), &pushConstants );

			vkCmdDrawIndexed( cmdBuffer, upload.indexCount, drawGroup->InstanceCount( surfIx ), upload.firstIndex, upload.vertexOffset, 0 );
		}	
		cmdContext->MarkerEndRegion();
	}

	vkCmdEndRendering( cmdBuffer );

	if( transitionState.flags.readAfter || transitionState.flags.presentAfter )
	{
		const gpuImageStateFlags_t colorNextState = transitionState.flags.presentAfter ? GPU_IMAGE_PRESENT : GPU_IMAGE_READ;
		for( uint32_t i = 0; i < colorAttachmentsCount; ++i )
		{
			Transition( cmdContext, *colorImages[ i ], GPU_IMAGE_WRITE, colorNextState );
		}
	}
	if( hasDepth && transitionState.flags.readAfter )
	{
		Transition( cmdContext, *fb->GetDepthStencil(), GPU_IMAGE_WRITE, GPU_IMAGE_READ );
	}
}


void RenderTask::Init( RenderView* view, drawPass_t begin, drawPass_t end )
{
	m_renderView = view;
	m_beginPass = begin;
	m_endPass= end;

	m_finishedSemaphore.Create( "Task Finished" );
}


void RenderTask::Shutdown()
{
	m_finishedSemaphore.Destroy();
}


void RenderTask::FrameBegin()
{
	if( m_renderView != nullptr ) {
		m_renderView->FrameBegin( m_beginPass, m_endPass );
	}
	GpuTask::OnFrameBegin();
}


void RenderTask::FrameEnd()
{
	if ( m_renderView != nullptr ) {
		m_renderView->FrameEnd( m_beginPass, m_endPass );
	}
}


void RenderTask::Resize()
{
	if ( m_renderView != nullptr ) {
		m_renderView->Resize();
	}
}


std::string RenderTask::AsString() const
{
	std::stringstream ss;
	ss << "RenderTask: " << m_renderView->GetName();
	return ss.str();
}


void RenderTask::Execute( CommandList& context )
{
	context.MarkerBeginRegion( m_renderView->GetName(), ColorToVector( Color::Cyan ) );

	const uint32_t multiViewCount = m_renderView->GetMultiViewCount();
	for( uint32_t multiViewIndex = 0; multiViewIndex < multiViewCount; ++multiViewIndex ) {
		RenderViewSurfaces( reinterpret_cast<GfxCmdList*>( &context ), multiViewIndex );
	}

	context.MarkerEndRegion();
}



