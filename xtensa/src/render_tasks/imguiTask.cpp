#include "imguiTask.h"
#include "../render_core/renderer.h"
#include "../render_state/cmdContext.h"
#include "../render_binding/bindings.h"
#include "../render_resources/gpuBuffer.h"
#include "../draw_passes/postPass.h"
#include "../asset_types/gpuProgram.h"
#include "../asset_types/assetLib.h"
#include "../globals/assetDefs.h"
#include "../app/imguiInterface.h"

#include "imageShaderTask.h"

struct imageStatParms_t
{
	vec4f		dimensions;
	vec2f		luminanceRange;
	vec2u		pickLocation;
	uint32_t	imageId;
	uint32_t	baseOffset;
};


struct imageStateReadback_t
{
	vec4f	pickLocationSample;
	vec2u	pickLocation;
};


static imageStateReadback_t s_readbackSample; // Just deal with a single sample for now
static imageViewerStatistics_t s_statsReadback;

#if defined( USE_IMGUI )
#include "../../../external/imgui/imgui.h"
#include "../../../external/imgui/backends/imgui_impl_glfw.h"
#include "../../../external/imgui/backends/imgui_impl_vulkan.h"

#include "../app/imguiInterface.h"

extern imguiControls_t g_imguiControls;
#endif

struct imguiTaskRenderData_t
{
	CommandList*		commandContext;
	RenderContext*		renderContext;
	DrawPass*			pass;
};

const uint32_t MaxImguiCallbackImages = 32;

static uint32_t pendingCallbackTasks = 0;
static imageViewerCallbackData_t callbackTasks[ MaxImguiCallbackImages ];
static imguiTaskRenderData_t renderTaskData; // Push/pop state

static float DequantizeFloat( const uint32_t u )
{
	const uint32_t mask = ( u >> 31 ) ? 0x80000000u : 0xFFFFFFFFu;
	const uint32_t bits = u ^ mask;
	float f;
	memcpy( &f, &bits, sizeof( f ) );
	return f;
};


bool QueryImageViewerSample( const vec2u& pickLocation, vec4f& outColor )
{
	if( ( s_readbackSample.pickLocation.x != pickLocation.x ) && ( s_readbackSample.pickLocation.y != pickLocation.y ) ) {
		return false;
	}
	outColor = s_readbackSample.pickLocationSample;
	return true;
}


bool QueryImageViewerStat( imageViewerStatistics_t& outStats )
{
	outStats.minSample = s_statsReadback.minSample;
	outStats.maxSample = s_statsReadback.maxSample;
	return true;
}


void ImguiImage2DRenderCallback( const ImDrawList* parentList, const ImDrawCmd* cmd )
{
	imageViewerCallbackData_t* callbackData = (imageViewerCallbackData_t*)cmd->UserCallbackData;

	if( cmd->ClipRect.x >= cmd->ClipRect.z || cmd->ClipRect.y >= cmd->ClipRect.w ) {
		return;
	}

	Asset<GpuProgram>* progAsset = GpuProgramLib().Find( "ImageViewer" );

	const Image* image = callbackData->image;

	const shaderPermId_t permSet = ( image->info.subsamples != IMAGE_SMP_1 ) ? shaderPermId_t::MSAA : shaderPermId_t::NONE;

	hdl_t pipeLineHdl = FindPipelineObject( renderTaskData.pass, *progAsset, permSet );

	if( pipeLineHdl == INVALID_HDL ) {
		pipeLineHdl = CreateGraphicsPipeline( renderTaskData.pass, *progAsset, permSet );
	}

	const vec4i clipRect = vec4i( (int32_t)cmd->ClipRect.x, (int32_t)cmd->ClipRect.y, (int32_t)cmd->ClipRect.z, (int32_t)cmd->ClipRect.w );

	vec4f extent{};
	extent.x = callbackData->x;
	extent.y = callbackData->y;
	extent.z = callbackData->width;
	extent.w = callbackData->height;
	
	scissor_t scissor{};
	scissor.x = clipRect.x;
	scissor.y = clipRect.y;
	scissor.width = ( clipRect.z - clipRect.x );
	scissor.height = ( clipRect.w - clipRect.y );
	renderTaskData.pass->SetScissor( clipRect.x, clipRect.y, clipRect.z - clipRect.x, clipRect.w - clipRect.y );

	vk_QuadDraw( *renderTaskData.commandContext, pipeLineHdl, extent, scissor, renderTaskData.pass );
}


void AddImageViewerCallback( ImDrawList* dl, const imageViewerCallbackData_t& callbackData )
{
	if ( pendingCallbackTasks >= MaxImguiCallbackImages ) {
		return;
	}

	callbackTasks[ pendingCallbackTasks ] = callbackData;

	dl->AddCallback( ImguiImage2DRenderCallback, &callbackTasks[ pendingCallbackTasks ] );
	dl->AddCallback( ImDrawCallback_ResetRenderState, nullptr );

	++pendingCallbackTasks;
}


void ImguiTask::Init( RenderContext* renderContext, ResourceContext* resourceContext, const frameBufferCreateInfo_t& fbInfo, const bool finalizeImage )
{
	m_context = renderContext;
	m_resources = resourceContext;

	m_transitionState.flags.presentAfter = finalizeImage;
	m_transitionState.flags.store = true;

	m_frameBuffer = new FrameBuffer();
	m_frameBuffer->Create( fbInfo );

	m_imagePass = new PostPass( m_context, m_frameBuffer );

	m_imagePass->parms = m_context->RegisterBindParm( "ImguiBindParms", bindset_imageShader);

	m_imagePass->codeImages.SetRenderContext( m_context );
	m_imagePass->codeCubeImages.SetRenderContext( m_context );

	m_imagePass->codeImages.Resize( 1 );
	m_imagePass->codeCubeImages.Resize( 1 );

	for ( uint32_t codeImageIx = 0; codeImageIx < 1; ++codeImageIx )
	{
		m_imagePass->codeImages.BindIndex( codeImageIx, rc.defaultImage );
		m_imagePass->codeCubeImages.BindIndex( codeImageIx, rc.defaultImageCube );
	}
	m_buffer.Create( "ImguiCallbackBuffer", swapBuffering_t::SINGLE_FRAME, resourceLifeTime_t::UNMANAGED, 1, MaxCustomConstantBytes, bufferType_t::UNIFORM );

	m_imageStatBuffer.Create(
		"ImageStatHistogram",
		swapBuffering_t::MULTI_FRAME,
		resourceLifeTime_t::UNMANAGED,
		2048,
		sizeof( uint32_t ),
		bufferType_t::READBACK );

	m_imageStatParmsBuffer.Create(
		"ImageStatParms",
		swapBuffering_t::SINGLE_FRAME,
		resourceLifeTime_t::UNMANAGED,
		1,
		sizeof( imageStatParms_t ),
		bufferType_t::UNIFORM );

	m_imageStatImages.SetRenderContext( m_context );
	m_imageStatImages.Resize( 1 );
	m_imageStatImages.BindIndex( 0, rc.defaultImage );

	m_imageStatParms = m_context->RegisterBindParm( "ImageStatsBindParms", m_context->LookupBindSet( bindset_compute ) );
	m_imageStatImage = nullptr;
	m_imageStatDispatched = false;
	memset( m_imageStatHistogram, 0, sizeof( m_imageStatHistogram ) );

	const Asset<GpuProgram>* progAsset = GpuProgramLib().Find( "ImageStat" );
	CreateComputePipeline( *progAsset );
}


void ImguiTask::Shutdown()
{
	if( m_imagePass != nullptr )
	{
		delete m_imagePass;
		m_imagePass = nullptr;
	}
	if( m_frameBuffer != nullptr )
	{
		delete m_frameBuffer;
		m_frameBuffer = nullptr;
	}
	m_buffer.Destroy();
	m_imageStatBuffer.Destroy();
	m_imageStatParmsBuffer.Destroy();
}


void ImguiTask::FrameBegin()
{
	struct viewerShaderConstants_t : public ImageShaderTask::baseConstants_t
	{
		vec4f		rectUv;
		vec4f		tint;
		float		rangeMin;
		float		rangeMax;
		uint32_t	flags;
		uint32_t	sampleIndex;
	};

	const viewport_t viewport = m_imagePass->GetViewport();

	m_imagePass->codeImages.BindIndex( 0, rc.redImage );
	m_imagePass->codeCubeImages.BindIndex( 0, rc.defaultImageCube );

	const Image* image = callbackTasks[ 0 ].image;

	if ( image != nullptr )
	{
		const bool isCubeImage = ( image->info.type == IMAGE_TYPE_CUBE );

		viewerShaderConstants_t constants{};

		constants.dimensions = vec4f( (float)image->info.width, (float)image->info.height, 1.0f / image->info.width, 1.0f / image->info.height );
		constants.level = callbackTasks[ 0 ].mipLevel;
		constants.mipCount = image->info.mipLevels;
		constants.layerCount = image->info.layers;
		constants.layer = callbackTasks[ 0 ].layer;
		constants.rectUv = vec4f( 0.0f, 0.0f, 1.0f, 1.0f );

		constants.tint = callbackTasks[ 0 ].tint;
		constants.rangeMin = callbackTasks[ 0 ].rangeMin;
		constants.rangeMax = callbackTasks[ 0 ].rangeMax;
		constants.flags = ( isCubeImage ? 0x01 : 0x00 ) | callbackTasks[ 0 ].flags;
		constants.sampleIndex = callbackTasks[ 0 ].msaaSampleIndex;

		m_buffer.SetPos( 0 );
		m_buffer.CopyData( &constants, sizeof( constants ) );

		if( isCubeImage ) {
			m_imagePass->codeCubeImages.BindIndex( 0, image );
		} else {
			m_imagePass->codeImages.BindIndex( 0, image );
		}		
	}

	m_imagePass->parms->Bind( BINDING_NAME( sourceImages ),		&m_imagePass->codeImages );
	m_imagePass->parms->Bind( BINDING_NAME( sourceCubeImages ),	m_imagePass->codeCubeImages[ 0 ] );
	m_imagePass->parms->Bind( BINDING_NAME( imageStencil ),		rc.whiteImage );
	m_imagePass->parms->Bind( BINDING_NAME( imageProcess ),		&m_buffer );

	// Image histogram setup — only run when a non-cube image is being viewed.
	m_imageStatImage      = nullptr;
	m_imageStatDispatched = false;

	if ( ( image != nullptr ) && ( image->info.type != IMAGE_TYPE_CUBE ) )
	{
		imageStatParms_t parms{};
		parms.dimensions = vec4f( (float)image->info.width, (float)image->info.height, (float)callbackTasks[ 0 ].mipLevel, 0.0f );
		parms.luminanceRange = vec2f( -8.0f, 4.0f );
		parms.imageId = 0;
		parms.baseOffset = 0;
		parms.pickLocation = vec2u( callbackTasks[ 0 ].pixelX, callbackTasks[ 0 ].pixelY );

		m_imageStatParmsBuffer.SetPos( 0 );
		m_imageStatParmsBuffer.CopyData( &parms, sizeof( parms ) );

		const uint32_t PickSampleOffset = 0;
		const uint32_t PickLocationOffset = 4;
		const uint32_t MinSampleOffset = 6;
		const uint32_t MaxSampleOffset = 10;

		// Readback, this is guaranteed complete, but has worst-case latency since it's the oldest frame
		assert( m_imageStatBuffer.VisibleToCpu() );

		m_imageStatBuffer.Invalidate();
		{
			const uint32_t* baseData = reinterpret_cast<uint32_t*>( m_imageStatBuffer.Get() );

			const vec4f* pickSample = reinterpret_cast<const vec4f*>( &baseData[ PickSampleOffset ] );
			const vec2u* pickLocation = reinterpret_cast<const vec2u*>( &baseData[ PickLocationOffset ] );

			s_readbackSample.pickLocationSample = *pickSample;
			s_readbackSample.pickLocation = *pickLocation;

			s_statsReadback.minSample = vec4f(	DequantizeFloat( baseData[ MinSampleOffset + 0 ] ), 
												DequantizeFloat( baseData[ MinSampleOffset + 1 ] ),
												DequantizeFloat( baseData[ MinSampleOffset + 2 ] ),
												DequantizeFloat( baseData[ MinSampleOffset + 3 ] ) );

			s_statsReadback.maxSample = vec4f(	DequantizeFloat( baseData[ MaxSampleOffset + 0 ] ),
												DequantizeFloat( baseData[ MaxSampleOffset + 1 ] ),
												DequantizeFloat( baseData[ MaxSampleOffset + 2 ] ),
												DequantizeFloat( baseData[ MaxSampleOffset + 3 ] ) );
		}

		memcpy( m_imageStatHistogram, m_imageStatBuffer.Get(), sizeof( m_imageStatHistogram ) );
		memset( m_imageStatBuffer.Get(), 0, ImageStatHistogramBins * sizeof( uint32_t ) );

		// Write global sentinels for the ordered-float min/max slots.
		// Min slots [6-9]  need QuantizeFloat(+FLT_MAX) = 0xFF7FFFFF so any real pixel wins InterlockedMin.
		// Max slots [10-13] need QuantizeFloat(-FLT_MAX) = 0x00800000 so any real pixel wins InterlockedMax.
		uint32_t* bufData = reinterpret_cast<uint32_t*>( m_imageStatBuffer.Get() );
		for( uint32_t i = MinSampleOffset; i < ( MinSampleOffset + 4 ); ++i ) {
			bufData[ i ] = 0xFF7FFFFFu;
		}
		for( uint32_t i = MaxSampleOffset; i < ( MaxSampleOffset + 4 ); ++i ) {
			bufData[ i ] = 0x00800000u;
		}

		m_imageStatBuffer.Flush();

		m_imageStatImages.BindIndex( 0, image, true );

		m_imageStatImage      = image;
		m_imageStatDispatched = true;
	}

	m_imageStatParms->Bind( BINDING_NAME( globalsBuffer ), &m_resources->globalConstants );
	m_imageStatParms->Bind( BINDING_NAME( computeImage ),  &m_imageStatImages );
	m_imageStatParms->Bind( BINDING_NAME( computeParms ),  &m_imageStatParmsBuffer );
	m_imageStatParms->Bind( BINDING_NAME( computeWrite ),  &m_imageStatBuffer );

	GpuTask::OnFrameBegin();

#ifdef USE_IMGUI
	// Prepare dear imgui render data
	ImGui::Render();
#endif
}


void ImguiTask::FrameEnd()
{
	m_imageStatDispatched = false;
}


void ImguiTask::Resize()
{
	if( ( m_frameBuffer != nullptr ) && ( m_frameBuffer->GetLifetime() == resourceLifeTime_t::RESIZE ) )
	{
		m_imagePass->SetFrameBuffer( m_frameBuffer );
	}
}


std::string ImguiTask::AsString() const
{
	std::stringstream ss;
	ss << "ImguiTask";
	return ss.str();
}


void ImguiTask::Execute( CommandList& cmdContext )
{
	cmdContext.MarkerBeginRegion( "ImGui", ColorToVector( Color::White ) );

	// Image histogram dispatch — must execute outside any rendering scope.
	if ( m_imageStatDispatched && m_imageStatImage != nullptr )
	{
		const uint32_t groupSize = 16;
		const uint32_t groupsX = CommandList::DispatchDim( m_imageStatImage->info.width, groupSize );
		const uint32_t groupsY = CommandList::DispatchDim( m_imageStatImage->info.width, groupSize );

		const Asset<GpuProgram>* progAsset = GpuProgramLib().Find( "ImageStat" );
		cmdContext.Dispatch( *progAsset, *m_imageStatParms, groupsX, groupsY, 1 );
	}

#ifdef USE_IMGUI
	const FrameBuffer* fb = m_frameBuffer;
	VkCommandBuffer cmdBuffer = cmdContext.CommandBuffer();

	VkRenderingAttachmentInfo colorAttachment = {};
	colorAttachment.sType       = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
	colorAttachment.imageView   = fb->GetColor()->gpuImage->GetVkImageView( context.bufferId );
	colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	colorAttachment.loadOp      = VK_ATTACHMENT_LOAD_OP_LOAD;
	colorAttachment.storeOp     = VK_ATTACHMENT_STORE_OP_STORE;

	const viewport_t& renderViewport = m_imagePass->GetViewport();

	VkRenderingInfo renderingInfo = {};
	renderingInfo.sType                = VK_STRUCTURE_TYPE_RENDERING_INFO;
	renderingInfo.renderArea.offset    = { renderViewport.x, renderViewport.y };
	renderingInfo.renderArea.extent    = { renderViewport.width, renderViewport.height };
	renderingInfo.layerCount           = 1;
	renderingInfo.colorAttachmentCount = 1;
	renderingInfo.pColorAttachments    = &colorAttachment;

	vkCmdBeginRendering( cmdBuffer, &renderingInfo );

	renderTaskData.commandContext = &cmdContext;
	renderTaskData.renderContext = m_context;
	renderTaskData.pass = m_imagePass;

	ImGui_ImplVulkan_RenderDrawData( ImGui::GetDrawData(), cmdBuffer );

	renderTaskData.commandContext = nullptr;
	renderTaskData.renderContext = nullptr;
	renderTaskData.pass = nullptr;

	pendingCallbackTasks = 0;

	vkCmdEndRendering( cmdBuffer );
#endif

	cmdContext.MarkerEndRegion();
}
