#include <algorithm>
#include <iterator>
#include <map>
#include "renderer.h"
#include "../scene/entity.h"
#include "../render_state/rhi.h"
#include "../render_binding/pipeline.h"
#include "../render_binding/bindings.h"
#include "../render_resources/imageArray.h"
#include "../render_tasks/RenderTask.h"
#include "../render_tasks/ImageReadbackTask.h"
#include "../render_tasks/imageMipTask.h"
#include "../render_tasks/imguiTask.h"
#include "../render_core/gpuTimerPool.h"

#include "../draw_passes/drawpass.h"
#include "swapChain.h"

#if defined( USE_IMGUI )
#include "../../../external/imgui/backends/imgui_impl_glfw.h"
#include "../../../external/imgui/backends/imgui_impl_vulkan.h"
#endif

#include "debugMenu.h"
#include "../globals/assetDefs.h"

#include "schedule.h"


void Renderer::Init( const renderConfig_t& initConfig )
{
	InitApi( initConfig );

	const renderConfig_t& config = renderContext.config;

	InitShaderResources();

	g_gpuTimerPool.Create();

	resources.gpuImages2D.SetRenderContext( &renderContext );
	resources.gpuImagesCube.SetRenderContext( &renderContext );

	resources.gpuImages2D.Resize( MaxImageDescriptors );
	resources.gpuImagesCube.Resize( MaxImageDescriptors );

	// Image samplers
	{
		samplerState_t samplerState{};
		samplerState.borderColor = SAMPLER_BORDER_BLACK;
		samplerState.borderColorIsFloat = true;
		samplerState.borderTransparent = false;
		samplerState.minLod = 0.0f;
		samplerState.maxLod = 16.0f;
		samplerState.maxAniso = 16.0f;

		samplerState.filter = SAMPLER_FILTER_NEAREST;
		resources.nearestSampler.Init( samplerState, resourceLifeTime_t::REBOOT );

		samplerState.filter = SAMPLER_FILTER_BILINEAR;

		for( uint32_t i = 0; i < SAMPLER_ADDRESS_MODES; ++i )
		{
			samplerState.addrMode = samplerAddress_t( i );

			samplerState.filter = samplerFilter_t::SAMPLER_FILTER_BILINEAR;
			resources.bilinearSamplers[ samplerState.addrMode ].Init( samplerState, resourceLifeTime_t::REBOOT );

			samplerState.filter = samplerFilter_t::SAMPLER_FILTER_TRILINEAR;
			resources.trilinearSamplers[ samplerState.addrMode ].Init( samplerState, resourceLifeTime_t::REBOOT );
		}

		samplerState.borderColor = SAMPLER_BORDER_WHITE;
		samplerState.borderColorIsFloat = true;
		samplerState.borderTransparent = false;
		samplerState.addrMode = samplerAddress_t::SAMPLER_ADDRESS_CLAMP_BORDER;
		samplerState.maxAniso = 0.0f;
		samplerState.pcf = true;

		resources.depthShadowSampler.Init( samplerState, resourceLifeTime_t::REBOOT );
	}

	viewCount = 0;

	// Shadow Views
	for ( uint32_t i = 0; i < MaxShadowViews; ++i )
	{
		renderViewCreateInfo_t info{};
		info.name = "Shadow View";
		info.viewMode = renderViewMode_t::SHADOW;
		info.viewId = viewCount;
		info.context = &renderContext;
		info.resources = &resources;
		info.isCubeView = ( resources.shadowMapImage[ i ]->info.type == imageType_t::IMAGE_TYPE_CUBE );

		info.clear = true;
		info.clearDepth = 1.0f;

		renderPassTransition_t& t = info.transition;		
		{
			t = {};
			t.flags.readBefore = true;
			t.flags.readAfter = true;
		}

		shadowViews[ i ] = &views[ viewCount ];
		shadowViews[ i ]->Init( info );
		++viewCount;
	}

	// Raster Views
	{
		renderViewCreateInfo_t info{};
		info.name = "Main View";
		info.viewMode = renderViewMode_t::FORWARD;
		info.viewId = viewCount;
		info.context = &renderContext;
		info.resources = &resources;

		info.clear = true;
		info.clearColor = vec4f( 0.0f, 0.5f, 0.5f, 1.0f );

		renderPassTransition_t& t = info.transition;
		{
			t = {};
			t.flags.readBefore = true;
			t.flags.readAfter = true;
		}

		renderViews[ 0 ] = &views[ viewCount ];
		renderViews[ 0 ]->Init( info );	
		++viewCount;
	}

	if ( config.useCubeViews )
	{
		{
			extern CVar r_cubeWidth;
			extern CVar r_cubeHeight;

			imageInfo_t colorInfo{};
			colorInfo.width = r_cubeWidth.GetInt();
			colorInfo.height = r_cubeHeight.GetInt();
			colorInfo.mipLevels = MipCount( colorInfo.width, colorInfo.height );
			colorInfo.layers = 6;
			colorInfo.subsamples = IMAGE_SMP_1;
			colorInfo.fmt = IMAGE_FMT_RGBA_16;
			colorInfo.type = IMAGE_TYPE_CUBE;
			colorInfo.tiling = IMAGE_TILING_MORTON;

			resources.cubeFbColorImage->Create(
				colorInfo,
				"FB_cubeColor", GPU_IMAGE_RW | GPU_IMAGE_TRANSFER, resourceLifeTime_t::REBOOT
			);

			resources.cubeFbColorImage->RegisterResize( nullptr );

			imageInfo_t depthInfo = colorInfo;
			depthInfo.fmt = IMAGE_FMT_D_16;

			resources.cubeFbDepthImage->Create(
				depthInfo,
				"FB_cubeDepth", GPU_IMAGE_RW | GPU_IMAGE_TRANSFER_SRC, resourceLifeTime_t::REBOOT
			);

			resources.cubeFbDepthImage->RegisterResize( nullptr );
		}

		renderViewCreateInfo_t info{};
		info.name = "Cube View";
		info.viewMode = renderViewMode_t::FORWARD;
		info.viewId = viewCount;
		info.context = &renderContext;
		info.resources = &resources;

		info.isCubeView = true;

		info.clear = true;
		info.clearColor = vec4f( 0.0f, 0.5f, 0.5f, 1.0f );

		renderPassTransition_t& t = info.transition;
		{
			t = {};
			t.flags.readBefore = true;
			t.flags.readAfter = true;
		}

		renderViews[ 1 ] = &views[ viewCount ];
		renderViews[ 1 ]->Init( info );
		++viewCount;
	}

	// 2D views
	{
		renderViewCreateInfo_t info{};
		info.name = "2D";
		info.viewMode = renderViewMode_t::DRAW_2D;
		info.viewId = viewCount;
		info.context = &renderContext;
		info.resources = &resources;

		info.clear = false;
		info.clearColor = vec4f( 0.0f, 0.5f, 0.5f, 1.0f );
		info.finalize = true;

		view2Ds[ 0 ] = &views[ viewCount ];
		view2Ds[ 0 ]->Init( info );

		++viewCount;
	}

	assert( viewCount <= ( MaxViews - 1 ) ); // Last view is reserved for temp views

	for ( uint32_t i = 0; i < MaxShadowViews; ++i ) {
		shadowViews[ i ]->Commit();
	}
	renderViews[ 0 ]->Commit();

	if( config.useCubeViews ) {
		renderViews[ 1 ]->Commit();
	}
	view2Ds[ 0 ]->Commit();

	// Workaround for ImGui
	// It requires an image to initialize, so a temp framebuffer is created
	// Framebuffers are lightweight and just encapsulate images (in this case the swapchain backbuffer)
	{
		frameBufferCreateInfo_t imguiFbInfo{};
		imguiFbInfo.name = "ImGuiInitFB";
		imguiFbInfo.context = &renderContext;
		imguiFbInfo.lifetime = resourceLifeTime_t::REBOOT;
		imguiFbInfo.swapBuffering = swapBuffering_t::MULTI_FRAME;
		imguiFbInfo.color0 = g_swapChain.GetBackBuffer();

		FrameBuffer* imguiFb = new FrameBuffer();
		imguiFb->Create( imguiFbInfo );

		InitImGui( imguiFb );
	}

	uploader.Boot( &renderContext, &resources );

#ifdef USE_VULKAN_RTX
	resources.tlas = uploader.GetAccelerationStructure();
#endif

	ClearPipelineCache();
	BuildPipelines();

	// Upload queue commands
	uploader.Upload();

	FlushGPU();
}


void Renderer::Destroy()
{
	g_gpuTimerPool.Destroy();

	RenderResource::Cleanup( resourceLifeTime_t::FRAME );
	RenderResource::Cleanup( resourceLifeTime_t::RESIZE );

	g_swapChain.Destroy();

	ShutdownImGui();

	// Buffers
	gfxContext.Destroy();
	computeContext.Destroy();

	ShutdownShaderResources();

	uploader.Shutdown();

	// Sync
	gfxContext.presentSemaphore.Destroy();
	gfxContext.renderFinishedSemaphore.Destroy();
	frameCompleteSemaphore.Destroy();
	computeContext.semaphore.Destroy();

	for( size_t i = 0; i < MaxFrameStates; ++i )
	{
		gfxContext.frameFence[ i ].Destroy();
	}

	schedule->Clear();

	AllocatorMemory::DestroyVmaAllocator();

	context.Destroy( g_window );

	g_window.~Window();
}


void Renderer::Shutdown()
{
	FlushGPU();
	Destroy();
}


void Renderer::BuildSchedule( TaskSchedule* schedule )
{
	RenderViewContext viewContext;
	viewContext.activeViews = &activeViews[ 0 ];
	viewContext.renderViews = &renderViews[ 0 ];
	viewContext.shadowViews = &shadowViews[ 0 ];
	viewContext.view2Ds = &view2Ds[ 0 ];

	BuildSceneSchedule( renderContext.config, &renderContext, &resources, &viewContext, uploader.GetGeometry(), schedule );
}


void Renderer::InitApi( const renderConfig_t& cfg )
{
	{
		// Device Set-up
		context.Create( g_window );

		InitConfig( cfg ); // Must be after device must be set-up, but before everything is initialized

		int width, height;
		g_window.GetWindowSize( width, height );
		g_swapChain.Create( &g_window, width, height );
	}

	{
		// Memory Allocations
		renderContext.sharedMemory.Create( MaxSharedMemory, memoryRegion_t::SHARED, resourceLifeTime_t::REBOOT );
		renderContext.localMemory.Create( MaxLocalMemory, memoryRegion_t::LOCAL, resourceLifeTime_t::REBOOT );
		renderContext.scratchMemory.Create( MaxScratchMemory, memoryRegion_t::LOCAL, resourceLifeTime_t::REBOOT );
	}

	{
		// Create Frame Resources
		renderContext.frameBufferMemory.Create( MaxFrameBufferMemory, memoryRegion_t::LOCAL, resourceLifeTime_t::RESIZE );

		CreateSyncObjects();
		CreateFramebuffers();

		gfxContext.Create( "GFX Context", &renderContext );
		computeContext.Create( "Compute Context", &renderContext );
	}

	{
		ShaderBindSet* bindset = nullptr;

		bindset = &renderContext.bindSets[ bindset_global ];
		bindset->Create( "GlobalBindings", g_globalBindings, COUNTARRAY( g_globalBindings ) );

		bindset = &renderContext.bindSets[ bindset_view ];
		bindset->Create( "ViewBindings", g_viewBindings, COUNTARRAY( g_viewBindings ) );

		bindset = &renderContext.bindSets[ bindset_pass ];
		bindset->Create( "PassBindings", g_passBindings, COUNTARRAY( g_passBindings ) );

		bindset = &renderContext.bindSets[ bindset_particle ];
		bindset->Create( "ParticleBindings", g_particleBindings, COUNTARRAY( g_particleBindings ) );

		bindset = &renderContext.bindSets[ bindset_compute ];
		bindset->Create( "ComputeBindings", g_computeBindings, COUNTARRAY( g_computeBindings ) );

		bindset = &renderContext.bindSets[ bindset_imageShader ];
		bindset->Create( "ImageProcessBindings", g_imageProcessBindings, COUNTARRAY( g_imageProcessBindings ) );

#ifdef USE_VULKAN_RTX
		bindset = &renderContext.bindSets[ bindset_rayTracing ];
		bindset->Create( "RtBindings", g_rtBindings, COUNTARRAY( g_rtBindings ) );
#endif
	}
}


void Renderer::AssignBindSetsToGpuProgs()
{
	const ShaderBindSet& globalBindSet = renderContext.bindSets[ bindset_global ];
	const ShaderBindSet& viewBindSet = renderContext.bindSets[ bindset_view ];
	const ShaderBindSet& passBindSet = renderContext.bindSets[ bindset_pass ];
	const ShaderBindSet& imageProcessBindSet = renderContext.bindSets[ bindset_imageShader ];

	{
		const uint32_t programCount = GpuProgramLib().Count();
		for ( uint32_t i = 0; i < programCount; ++i )
		{
			GpuProgram& prog = GpuProgramLib().Find( i )->Get();

			prog.bindsetCount = 0;

			if ( prog.type == pipelineType_t::RASTER )
			{
				prog.bindsets[ prog.bindsetCount ] = &globalBindSet;
				prog.bindsetCount += 1;

				if ( ( prog.flags & shaderFlags_t::IMAGE_SHADER ) == shaderFlags_t::NONE )
				{
					prog.bindsets[ prog.bindsetCount ] = &viewBindSet;
					prog.bindsetCount += 1;
				}

				{
					auto it = renderContext.bindSets.find( prog.bindHash );
					if ( it != renderContext.bindSets.end() ) {
						prog.bindsets[ prog.bindsetCount ] = &it->second;
					} else {
						prog.bindsets[ prog.bindsetCount ] = &passBindSet;
					}
					prog.bindsetCount += 1;
				}
			}
#ifdef USE_VULKAN_RTX
			else if ( prog.type == pipelineType_t::RAY_TRACING )
			{
				prog.bindsets[ prog.bindsetCount ] = &globalBindSet;
				prog.bindsetCount += 1;

				auto it = renderContext.bindSets.find( bindset_rayTracing );
				if ( it != renderContext.bindSets.end() )
				{
					prog.bindsets[ prog.bindsetCount ] = &it->second;
					prog.bindsetCount += 1;
				}
			}
#endif
			else
			{
				// COMPUTE and other pipeline types: single custom bindset
				auto it = renderContext.bindSets.find( prog.bindHash );
				if ( it != renderContext.bindSets.end() ) {
					prog.bindsets[ prog.bindsetCount ] = &it->second;
				} else {
					prog.bindsets[ prog.bindsetCount ] = &passBindSet;
				}
				prog.bindsetCount += 1;
			}
		}
	}
}


void Renderer::InitShaderResources()
{
	const ShaderBindSet& globalBindSet = renderContext.bindSets[ bindset_global ];
	const ShaderBindSet& particleBindSet = renderContext.bindSets[ bindset_particle ];

	renderContext.globalParms = renderContext.RegisterBindParm( "GlobalBindParms", & globalBindSet);

	{
		particleState.parms = renderContext.RegisterBindParm( "ParticleBindParms", &particleBindSet );
		particleState.x = ( MaxParticles / 256 );
	}

	{
		rc.redImage = &ImageLib().Find( "_red" )->Get();
		rc.blueImage = &ImageLib().Find( "_green" )->Get();
		rc.greenImage = &ImageLib().Find( "_blue" )->Get();
		rc.whiteImage = &ImageLib().Find( "_white" )->Get();
		rc.blackImage = &ImageLib().Find( "_black" )->Get();
		rc.lightGreyImage = &ImageLib().Find( "_lightGrey" )->Get();
		rc.darkGreyImage = &ImageLib().Find( "_darkGrey" )->Get();
		rc.brownImage = &ImageLib().Find( "_brown" )->Get();
		rc.cyanImage = &ImageLib().Find( "_cyan" )->Get();
		rc.yellowImage = &ImageLib().Find( "_yellow" )->Get();
		rc.purpleImage = &ImageLib().Find( "_purple" )->Get();
		rc.orangeImage = &ImageLib().Find( "_orange" )->Get();
		rc.pinkImage = &ImageLib().Find( "_pink" )->Get();
		rc.goldImage = &ImageLib().Find( "_gold" )->Get();
		rc.albImage = &ImageLib().Find( "_alb" )->Get();
		rc.nmlImage = &ImageLib().Find( "_nml" )->Get();
		rc.rghImage = &ImageLib().Find( "_rgh" )->Get();
		rc.mtlImage = &ImageLib().Find( "_mtl" )->Get();
		rc.defaultImage = &ImageLib().Find( "_default" )->Get();
		rc.defaultImageCube = &ImageLib().Find( "_defaultCube" )->Get();

		rc.defaultImageArray.SetRenderContext( &renderContext );
		rc.defaultImageArray.Resize( 1 );
		rc.defaultImageArray.BindIndex( 0, rc.defaultImage );

		rc.defaultImageCubeArray.SetRenderContext( &renderContext );
		rc.defaultImageCubeArray.Resize( 1 );
		rc.defaultImageCubeArray.BindIndex( 0, rc.defaultImageCube );

		{
			imageInfo_t info{};
			info.width = 1;
			info.height = 1;
			info.mipLevels = 1;
			info.layers = 1;
			info.subsamples = IMAGE_SMP_1;
			info.fmt = IMAGE_FMT_R_16;
			info.type = IMAGE_TYPE_2D;
			info.tiling = IMAGE_TILING_MORTON;

			rc.defaultStorageImage.Create(
				info,
				"FB_defaultStorage", GPU_IMAGE_STORAGE | GPU_IMAGE_RW, resourceLifeTime_t::RESIZE
			);
		}
	}

	// Buffers
	{
		resources.globalConstants.Create( 
			"Globals",
			swapBuffering_t::MULTI_FRAME,
			resourceLifeTime_t::REBOOT,
			1,
			sizeof( gpuGlobals_t ),
			bufferType_t::UNIFORM
		);
		resources.viewParms.Create(
			"View",
			swapBuffering_t::MULTI_FRAME,
			resourceLifeTime_t::REBOOT,
			MaxViews * MaxMultiViews,
			sizeof( gpuView_t ),
			bufferType_t::STORAGE
		);
		resources.surfParms.Create(
			"Surf",
			swapBuffering_t::MULTI_FRAME,
			resourceLifeTime_t::REBOOT,
			MaxViews * MaxSurfaces,
			sizeof( gpuSurface_t ),
			bufferType_t::STORAGE
		);
		resources.materialBuffers.Create(
			"Material",
			swapBuffering_t::MULTI_FRAME,
			resourceLifeTime_t::REBOOT,
			MaxMaterials,
			sizeof( gpuMaterial_t ),
			bufferType_t::STORAGE
		);
		resources.lightParms.Create(
			"Light",
			swapBuffering_t::MULTI_FRAME,
			resourceLifeTime_t::REBOOT,
			MaxLights,
			sizeof( gpuLight_t ),
			bufferType_t::STORAGE
		);
		resources.particleBuffer.Create(
			"Particle",
			swapBuffering_t::MULTI_FRAME,
			resourceLifeTime_t::REBOOT,
			MaxParticles,
			sizeof( gpuParticle_t ),
			bufferType_t::STORAGE
		);
	}
}


void Renderer::ShutdownShaderResources()
{
	// Managed Cleanup
	RenderResource::Cleanup( resourceLifeTime_t::REBOOT );

	// Images
	const uint32_t textureCount = ImageLib().Count();
	for( uint32_t i = 0; i < textureCount; ++i )
	{
		const Image& texture = ImageLib().Find( i )->Get();
		delete texture.gpuImage;
	}

	for( uint32_t i = 0; i < MaxImageDescriptors; ++i )
	{
		resources.gpuImages2D.BindIndex( i, nullptr );
		resources.gpuImagesCube.BindIndex( i, nullptr );
	}

	// PSO
	DestroyPipelineCache();

	const uint32_t shaderCount = GpuProgramLib().Count();
	for( uint32_t i = 0; i < shaderCount; ++i )
	{
		GpuProgram& prog = GpuProgramLib().Find( i )->Get();

		prog.DestroyApiObjects();
	}

	renderContext.FreeRegisteredBindParms();
}


void Renderer::InitImGui( const FrameBuffer* fb )
{
#if defined( USE_IMGUI )
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	// Setup Platform/Renderer bindings

#if defined( USE_VULKAN ) && defined( USE_GLFW )
	ImGui_ImplGlfw_InitForVulkan( g_window.window, true );
#endif

	assert( fb != nullptr );

	ImGui_ImplVulkan_InitInfo vkInfo = {};
	vkInfo.ApiVersion = VK_API_VERSION_1_2;
	vkInfo.Instance = context.instance;
	vkInfo.PhysicalDevice = context.physicalDevice;
	vkInfo.Device = context.device;
	vkInfo.QueueFamily = context.queueFamilyIndices[ QUEUE_GRAPHICS ];
	vkInfo.Queue = context.gfxContext;
	vkInfo.PipelineCache = nullptr;
	vkInfo.DescriptorPool = context.descriptorPool;
	vkInfo.Allocator = nullptr;
	vkInfo.MinImageCount = MaxFrameStates;
	vkInfo.ImageCount = MaxFrameStates;
	vkInfo.CheckVkResultFn = nullptr;
	vkInfo.UseDynamicRendering = true;
#ifdef USE_VULKAN
#ifdef IMGUI_IMPL_VULKAN_HAS_DYNAMIC_RENDERING
	{
		const renderAttachmentBits_t    attachBits = fb->GetAttachmentBits();
		const renderPassAttachmentMask_t attachMask = fb->GetAttachmentMask();
		static VkFormat colorFmt = ( attachMask & RENDER_PASS_MASK_COLOR0 ) ? vk_GetTextureFormat( attachBits.color0.fmt ) : VK_FORMAT_UNDEFINED;

		vkInfo.PipelineInfoMain.PipelineRenderingCreateInfo.sType                   = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
		vkInfo.PipelineInfoMain.PipelineRenderingCreateInfo.colorAttachmentCount    = ( colorFmt != VK_FORMAT_UNDEFINED ) ? 1 : 0;
		vkInfo.PipelineInfoMain.PipelineRenderingCreateInfo.pColorAttachmentFormats = &colorFmt;
	}
#endif
#endif

#ifdef USE_VULKAN
	ImGui_ImplVulkan_Init( &vkInfo );
#endif

	// Setup Dear ImGui style
	ImGui::StyleColorsDark();
#ifdef USE_GLFW
	ImGui_ImplGlfw_NewFrame();
#endif
#endif
}


void Renderer::ShutdownImGui()
{
#if defined( USE_IMGUI )
#ifdef USE_VULKAN
	ImGui_ImplVulkan_Shutdown();
#endif
#ifdef USE_GLFW
	ImGui_ImplGlfw_Shutdown();
#endif
	ImGui::DestroyContext();
#endif
}


void Renderer::BuildPipelines()
{
	const uint32_t programCount = GpuProgramLib().Count();

	std::vector< Asset<GpuProgram>* > invalidAssets;
	invalidAssets.reserve( programCount );

	// 1. Collect stale shaders	
	for ( uint32_t progIx = 0; progIx < programCount; ++progIx )
	{
		Asset<GpuProgram>* progAsset = GpuProgramLib().Find( progIx );
		if ( progAsset == nullptr ) {
			continue;
		}

		if ( progAsset->IsUploaded() ) {
			continue;
		}
		invalidAssets.push_back( progAsset );
	}

	if( invalidAssets.size() == 0 ) {
		return;
	}

	FlushGPU();

	// 2. Assign the bind sets (i.e. descriptor set layouts)
	AssignBindSetsToGpuProgs();

	// 3. Recreate shaders
	for ( auto it = invalidAssets.begin(); it != invalidAssets.end(); ++it )
	{
		Asset<GpuProgram>* progAsset = *it;
		GpuProgram& prog = progAsset->Get();

		prog.DestroyApiObjects();
		prog.CreateApiObjects();
	}

	// 4. Rebuild Pipelines
	// This does nothing on first load. The rest of the renderer will take care of actually building the pipeline
	// The rederer needs additional state (e.g. blend modes), that make it hard to build a priori
	// The upload mechanism is there mostly to trigger the upload process, unfortunately this is complex for shaders
	for( auto it = invalidAssets.begin(); it != invalidAssets.end(); ++it )
	{
		Asset<GpuProgram>* progAsset = *it;
		DestoryAllPipelines( *progAsset );
		progAsset->CompleteUpload();
	}
}


void Renderer::CreateFramebuffers()
{
	int width = 0;
	int height = 0;
	g_window.QueryWindowFrameBufferSize( width, height );

	renderContext.displayWidth = width;
	renderContext.displayHeight = height;

	resources.RegisterOutputImages();

	// TODO: Force all FrameBuffers to be resize for now
	// TODO: Need new function or have resize callback/function that adjusts dimentions if marked as RESIZE
	const resourceLifeTime_t lifeTime = resourceLifeTime_t::RESIZE;

	// Shadow images
	for( uint32_t shadowIx = 0; shadowIx < MaxShadowMaps; ++shadowIx )
	{
		imageInfo_t info{};
		info.width = Renderer::ShadowMapWidth;
		info.height = Renderer::ShadowMapHeight;
		info.mipLevels = 1;
		info.layers = 1;
		info.subsamples = IMAGE_SMP_1;
		info.fmt = IMAGE_FMT_D_32;
		info.type = IMAGE_TYPE_2D;
		info.tiling = IMAGE_TILING_MORTON;

		Image* shadowImage = resources.shadowMapImage[ shadowIx ];

		shadowImage->Create(
			info,
			"FB_shadowMap", GPU_IMAGE_RW, lifeTime
		);

		shadowImage->RegisterResize( nullptr );
	}

	// Main Scene Render Images
	{
		imageInfo_t info{};
		info.width = width;
		info.height = height;
		info.mipLevels = 1;
		info.layers = 1;
		info.subsamples = renderContext.config.mainColorSubSamples;
		info.fmt = IMAGE_FMT_RGBA_16;
		info.type = IMAGE_TYPE_2D;
		info.tiling = IMAGE_TILING_MORTON;

		resources.mainColorImage->Create(
			info,
			"FB_mainColor", GPU_IMAGE_RW | GPU_IMAGE_TRANSFER_SRC, lifeTime
		);
		
		resources.gBufferLayerImage0->Create(
			info,
			"FB_gBufferLayer", GPU_IMAGE_RW | GPU_IMAGE_TRANSFER_SRC, lifeTime
		);

		resources.gBufferLayerImage1->Create(
			info,
			"FB_gBufferLayer", GPU_IMAGE_RW | GPU_IMAGE_TRANSFER_SRC, lifeTime
		);

		info.fmt = IMAGE_FMT_RG_32_UINT;

		resources.visBufferImage->Create(
			info,
			"FB_visBuffer", GPU_IMAGE_RW, lifeTime
		);

		info.fmt = IMAGE_FMT_D_32_S8;
		info.type = IMAGE_TYPE_2D;

		resources.depthStencilImage->Create(
			info,
			"FB_viewDepth", GPU_IMAGE_RW | GPU_IMAGE_TRANSFER_SRC, lifeTime
		);
	}

	// Post-Scene Render Images
	{
		imageInfo_t info{};
		info.width = width;
		info.height = height;
		info.mipLevels = MipCount( info.width, info.height );
		info.layers = 1;
		info.subsamples = IMAGE_SMP_1;
		info.fmt = resources.mainColorImage->info.fmt;
		info.type = IMAGE_TYPE_2D;
		info.tiling = resources.mainColorImage->info.tiling;

		resources.mainColorResolvedImage->Create(
			info,
			"FB_mainColorResolvedImage", GPU_IMAGE_RW | GPU_IMAGE_TRANSFER, lifeTime
		);

		info.fmt = resources.gBufferLayerImage0->info.fmt;
		resources.gBufferLayerResolvedImage0->Create(
			info,
			"FB_gBufferLayerResolvedImage0", GPU_IMAGE_RW | GPU_IMAGE_TRANSFER, lifeTime
		);

		info.fmt = resources.gBufferLayerImage1->info.fmt;
		resources.gBufferLayerResolvedImage1->Create(
			info,
			"FB_gBufferLayerResolvedImage1", GPU_IMAGE_RW | GPU_IMAGE_TRANSFER, lifeTime
		);

		resources.blurredImage->Create(
			info,
			"FB_blurredImage", GPU_IMAGE_RW | GPU_IMAGE_TRANSFER, lifeTime
		);
	}

	// Depth-stencil views
	{
		imageSubResourceView_t depthView = resources.depthStencilImage->subResourceView;
		depthView.aspect = IMAGE_ASPECT_DEPTH_FLAG;
		resources.depthImageView.Init( resources.depthStencilImage, resources.depthStencilImage->info, depthView, lifeTime );

		imageSubResourceView_t stencilView = resources.depthStencilImage->subResourceView;
		stencilView.aspect = IMAGE_ASPECT_STENCIL_FLAG;
		resources.stencilImageView.Init( resources.depthStencilImage, resources.depthStencilImage->info, stencilView, lifeTime );
	}

	// Resolve depth-stencil image
	{
		imageInfo_t info{};
		info.width = width;
		info.height = height;
		info.mipLevels = 1;
		info.layers = 1;
		info.subsamples = IMAGE_SMP_1;
		info.fmt = IMAGE_FMT_RG_32;
		info.type = IMAGE_TYPE_2D;
		info.tiling = resources.depthStencilImage->info.tiling;

		resources.depthStencilResolvedImage->Create(
			info,
			"FB_depthStencilResolvedImage", GPU_IMAGE_RW, lifeTime
		);
	}

	// Depth-stencil views
	{
		resources.depthResolvedImageView.Init( resources.depthStencilResolvedImage, resources.depthStencilResolvedImage->info, lifeTime );
		resources.stencilResolvedImageView.Init( resources.depthStencilResolvedImage, resources.depthStencilResolvedImage->info, lifeTime );
	}

	// Temp image
	{
		imageInfo_t info{};
		info.width = width;
		info.height = height;
		info.mipLevels = MipCount( width, height );
		info.layers = 1;
		info.subsamples = IMAGE_SMP_1;
		info.fmt = IMAGE_FMT_RGBA_16;
		info.type = IMAGE_TYPE_2D;
		info.tiling = IMAGE_TILING_MORTON;

		resources.tempColorImage->Create(
			info,
			"FB_tempColor", GPU_IMAGE_RW, lifeTime
		);
	}

	// Aliasable heap for temp images
	// Conservative sizing so aliased images fit within the space
	{
		imageInfo_t heapRef{};
		heapRef.width = width;
		heapRef.height = height;
		heapRef.mipLevels = 1;
		heapRef.layers = 1;
		heapRef.subsamples = IMAGE_SMP_1;
		heapRef.fmt = IMAGE_FMT_RGBA_32;
		heapRef.type = IMAGE_TYPE_2D;
		heapRef.tiling = IMAGE_TILING_MORTON;

		resources.tempColorImageHeap.Create( "FB_TempColorHeap", heapRef, GPU_IMAGE_RW, resourceLifeTime_t::REBOOT );
	}

	// Luminance MIP-chain
	{
		imageInfo_t info{};
		info.width = 1024;
		info.height = 1024;
		info.mipLevels = MipCount( info.width, info.height );
		info.layers = 1;
		info.subsamples = IMAGE_SMP_1;
		info.fmt = IMAGE_FMT_R_16;
		info.type = IMAGE_TYPE_2D;
		info.tiling = IMAGE_TILING_MORTON;

		resources.currentLum->Create(
			info,
			"FB_currentLuminance", GPU_IMAGE_RW | GPU_IMAGE_TRANSFER_SRC, lifeTime
		);

		resources.currentLum->RegisterResize( nullptr );
	}
}


void Renderer::CreateSyncObjects()
{
	gfxContext.presentSemaphore.Create( "PresentSemaphore" );
	gfxContext.renderFinishedSemaphore.Create( "RenderSemaphore" );
	computeContext.semaphore.Create( "ComputeSemaphore" );
	frameCompleteSemaphore.Create( "FrameCompleteTimeline" );

	for ( size_t i = 0; i < MaxFrameStates; ++i ) {
		gfxContext.frameFence[ i ].Create( "FrameFence" );
	}

#ifdef USE_VULKAN
	gfxContext.presentSemaphore.waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	frameCompleteSemaphore.waitStage = static_cast<VkPipelineStageFlagBits>( VK_PIPELINE_STAGE_TRANSFER_BIT | VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR );
#endif
}
