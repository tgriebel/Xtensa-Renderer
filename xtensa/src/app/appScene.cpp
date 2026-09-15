#include "../globals/common.h"
#include <gfxcore/core/util.h>
#include "io.h"
#include "window.h"
#include "../scene/entity.h"
#include "../scene/sceneBase.h"
#include "../globals/render_util.h"

#if defined( USE_IMGUI )
#include "../../../external/imgui/imgui_internal.h"
#include "../../../external/imgui/imgui.h"
#include "imguiInterface.h"
#endif

#include "../render_core/log.h"
#include "../render_core/debugMenu.h"
#include "../render_core/gpuTimerPool.h"
#include "../render_core/schedule.h"
#include "../render_core/renderer.h"
#include "../render_core/allocator.h"
#include "../globals/assetDefs.h"

extern Scene* g_scene;
extern Renderer g_renderer;

#if defined( USE_IMGUI )
extern imguiControls_t g_imguiControls;
#endif
extern Window					g_window;

void DrawSceneDebugMenu();
void DrawPostProcessDebugMenu();
void DrawAssetDebugMenu();
void DrawManipDebugMenu();
void DrawEntityDebugMenu();
void DrawOutlinerDebugMenu();
void DrawDrawGroupDebugMenu();
void DeviceDebugMenu();

void InitScene( Scene* scene )
{
	// FIXME: weird left-over stuff from refactors
	const uint32_t entCount = static_cast<uint32_t>( scene->entities.size() );
	for ( uint32_t i = 0; i < entCount; ++i ) {
		scene->CreateEntityBounds( scene->entities[ i ]->modelHdl, *scene->entities[ i ] );

		scene->entities[ i ]->envMap = ImageLib().RetrieveHdl( scene->envMap.c_str() );
		scene->entities[ i ]->diffuseIblMap = ImageLib().RetrieveHdl( scene->diffuseIblMap.c_str() );
	}

	scene->Init();

	scene->mainCamera->Translate( vec4f( 0.0f, 1.66f, 1.0f, 0.0f ) );

	{
		Entity* ent = new Entity();
		scene->CreateEntityBounds( ModelLib().RetrieveHdl( "_postProcessQuad" ), *ent );
		ent->name = "_postProcessQuad";
		scene->entities.push_back( ent );
	}

	{
		Entity* ent = new Entity();
		scene->CreateEntityBounds( ModelLib().RetrieveHdl( "_quadTexDebug" ), *ent );
		ent->name = "_quadTexDebug";
		scene->entities.push_back( ent );
	}

	// Sky Entity
	{
		hdl_t skyBox = ModelLib().AddDeferred( "_skybox", loader_t( new SkyBoxLoader() ) );
		Asset<Model>* skyModelAsset = ModelLib().Find( skyBox );
		skyModelAsset->Load();

		Material& skyMaterial = MaterialLib().Find( "_sky" )->Get();
		skyMaterial.AddTexture( 0, ImageLib().RetrieveHdl( scene->envMap.c_str() ) );

		Entity* ent = new Entity();
		scene->CreateEntityBounds( ModelLib().RetrieveHdl( "_skybox" ), *ent );
		ent->name = "_sky";
		ent->SetFlag( ENT_FLAG_CAMERA_LOCKED );
		scene->entities.push_back( ent );
	}

	{
		scene->lights.resize( 3 );
		scene->lights[ 0 ].pos = vec4f( 0.0f, 0.0f, 6.0f, 0.0f );
		scene->lights[ 0 ].intensity = 1.0f;
		scene->lights[ 0 ].dir = vec4f( 0.0f, 0.0f, -1.0f, 0.0f );
		scene->lights[ 0 ].color = Color::White;
		scene->lights[ 0 ].flags = LIGHT_FLAGS_SHADOW;

		scene->lights[ 1 ].pos = vec4f( 0.0f, 10.0f, 5.0f, 0.0f );
		scene->lights[ 1 ].intensity = 1.0f;
		scene->lights[ 1 ].dir = vec4f( 0.0f, 0.0f, -1.0f, 0.0f );
		scene->lights[ 1 ].color = Color::Red;
		scene->lights[ 1 ].flags = LIGHT_FLAGS_SHADOW | LIGHT_FLAGS_POINT;

		scene->lights[ 2 ].pos = vec4f( 0.0f, -10.0f, 5.0f, 0.0f );
		scene->lights[ 2 ].intensity = 1.0f;
		scene->lights[ 2 ].dir = vec4f( 0.0f, 0.0f, -1.0f, 0.0f );
		scene->lights[ 2 ].color = Color::Blue;
		scene->lights[ 2 ].flags = LIGHT_FLAGS_SHADOW;
	}

	{
		g_imguiControls.raytraceScene = false;
		g_imguiControls.rasterizeScene = false;
		g_imguiControls.rebuildRaytraceScene = false;
		g_imguiControls.rebuildShaders = false;
		g_imguiControls.shaderHdl = INVALID_HDL;
		g_imguiControls.heightMapHeight = 1.0f;
		g_imguiControls.pbr.roughnessScale = 1.0f;
		g_imguiControls.pbr.roughnessBias = 0.0f;
		g_imguiControls.pbr.metalnessScale = 1.0f;
		g_imguiControls.pbr.metalnessBias = 0.0f;
		g_imguiControls.pbr.useDiffuseIBL = true;
		g_imguiControls.pbr.useSpecularIBL = true;
		g_imguiControls.pbr.debugLightingMode = DEBUG_NONE;
		g_imguiControls.shadowStrength = 0.99f;
		g_imguiControls.postProcess.toneMapColor[ 0 ] = 1.0f;
		g_imguiControls.postProcess.toneMapColor[ 1 ] = 1.0f;
		g_imguiControls.postProcess.toneMapColor[ 2 ] = 1.0f;
		g_imguiControls.postProcess.toneMapColor[ 3 ] = 1.0f;
		g_imguiControls.postProcess.bloomEnable = true;
		g_imguiControls.postProcess.bloomBlendWeight = 0.004f;
		g_imguiControls.postProcess.autoExposureEnable = true;
		g_imguiControls.postProcess.exposureMidGray = 0.18f;
		g_imguiControls.postProcess.exposureAdaptation = 1.0f;
		g_imguiControls.postProcess.exposureWhitePoint = 1.0f;
		g_imguiControls.postProcess.exposureDarkLimit = 0.0005f;
		g_imguiControls.postProcess.caEnable = true;
		g_imguiControls.postProcess.caIntensity = 0.01f;
		g_imguiControls.postProcess.dofEnable = false;
		g_imguiControls.dbgImageId = -1;
		g_imguiControls.selectedFrameBufferImageId = -1;
		g_imguiControls.isTextured = true;
		g_imguiControls.selectedEntityId = -1;
		g_imguiControls.selectedModelOrigin = vec3f( 0.0f );
	}
}


void ShutdownScene( Scene* scene )
{
	const uint32_t entCount = static_cast<uint32_t>( scene->entities.size() );
	for( uint32_t i = 0; i < entCount; ++i )
	{
		delete scene->entities[i];
	}
}


void UpdateScene( Scene* scene )
{

#if defined( USE_IMGUI )
	ImGui::NewFrame();
	ImGui::DockSpaceOverViewport( 0, ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode );
#endif

	const float dt = scene->DeltaTime();
	const float cameraSpeed = 5.0f;

	static float smoothDeltaTime = 0.0f;
	smoothDeltaTime = 0.9f * smoothDeltaTime + 0.1f * dt;

	// Key controls
	{
		if ( g_window.input.IsKeyPressed( KEY_D ) ) {
			scene->mainCamera->Truck( cameraSpeed * dt );
		}
		if ( g_window.input.IsKeyPressed( KEY_A ) ) {
			scene->mainCamera->Truck( -cameraSpeed * dt );
		}
		if ( g_window.input.IsKeyPressed( KEY_W ) ) {
			scene->mainCamera->Dolly( cameraSpeed * dt );
		}
		if ( g_window.input.IsKeyPressed( KEY_S ) ) {
			scene->mainCamera->Dolly( -cameraSpeed * dt );
		}
		if ( g_window.input.IsKeyPressed( KEY_1 ) )
		{
			scene->mainCamera->SetAngles( vec3f( 0.0f, 0.0f, 0.0f ) );
			scene->mainCamera->Pan( 0.0f * PI );
		}
		if ( g_window.input.IsKeyPressed( KEY_2 ) )
		{
			scene->mainCamera->SetAngles( vec3f( 0.0f, 0.0f, 0.0f ) );
			scene->mainCamera->Pan( 0.5f * PI );
		}
		if ( g_window.input.IsKeyPressed( KEY_3 ) )
		{
			scene->mainCamera->SetAngles( vec3f( 0.0f, 0.0f, 0.0f ) );
			scene->mainCamera->Pan( 1.0f * PI );
		}
		if ( g_window.input.IsKeyPressed( KEY_4 ) )
		{
			scene->mainCamera->SetAngles( vec3f( 0.0f, 0.0f, 0.0f ) );
			scene->mainCamera->Pan( 1.5f * PI );
		}
		if ( g_window.input.IsKeyPressed( KEY_5 ) )
		{
			scene->mainCamera->SetAngles( vec3f( 0.0f, 0.0f, 0.0f ) );
			scene->mainCamera->Tilt( -0.5f * PI );
		}
		if ( g_window.input.IsKeyPressed( KEY_6 ) )
		{
			scene->mainCamera->SetAngles( vec3f( 0.0f, 0.0f, 0.0f ) );
			scene->mainCamera->Tilt( 0.5f * PI );
		}
		if ( g_window.input.IsKeyPressed( KEY_ADD ) ) {
			scene->mainCamera->SetFov( scene->mainCamera->GetFov() + Radians( 0.1f ), g_window.QueryWindowFrameBufferAspect() );
		}
		if ( g_window.input.IsKeyPressed( KEY_SUB ) ) {
			scene->mainCamera->SetFov( scene->mainCamera->GetFov() - Radians( 0.1f ), g_window.QueryWindowFrameBufferAspect() );
		}
	}
	scene->mainCamera->SetFov( scene->mainCamera->GetFov(), g_window.QueryWindowFrameBufferAspect() );

	const mouse_t& mouse = g_window.input.GetMouse();
	if ( mouse.centered )
	{
		const float maxSpeed = mouse.speed * smoothDeltaTime;
		const float yawDelta = -maxSpeed * mouse.dx;
		const float pitchDelta = maxSpeed * mouse.dy;
		scene->mainCamera->Pan( yawDelta );
		scene->mainCamera->Tilt( pitchDelta );
	}
	else if ( mouse.leftDown )
	{
		Ray ray = scene->mainCamera->GetViewRay( vec2f( 0.5f * mouse.x + 0.5f, 0.5f * mouse.y + 0.5f ) );
		scene->selectedEntity = scene->GetTracedEntity( ray );
	}

	scene->camera2D.SetAngles( vec3f( 0.0f, 0.0f, 0.0f ) );
	scene->camera2D.SetClip( -1.0f, 1.0f );

	const uint32_t entCount = static_cast<uint32_t>( scene->entities.size() );
	for ( uint32_t i = 0; i < entCount; ++i )
	{
		scene->entities[ i ]->envMap = ImageLib().RetrieveHdl( scene->specIblMap.c_str() );
		scene->entities[ i ]->diffuseIblMap = ImageLib().RetrieveHdl( scene->diffuseIblMap.c_str() );
	}

	{
		Entity* ent = scene->FindEntity( "_quadTexDebug" );
		if ( ent != nullptr ) {
			ent->SetFlag( ENT_FLAG_NO_DRAW );
		}
	}

#if defined( USE_IMGUI )
	if ( ImGui::BeginMainMenuBar() )
	{
		if ( ImGui::BeginMenu( "File" ) )
		{
			if ( ImGui::MenuItem( "Open Scene", "CTRL+O" ) ) {
				g_imguiControls.openSceneFileDialog = true;
			}
			if ( ImGui::MenuItem( "Reload", "CTRL+R" ) ) {
				g_imguiControls.reloadScene = true;
			}
			if ( ImGui::MenuItem( "Import Obj", "CTRL+I" ) ) {
				g_imguiControls.openModelImportFileDialog = true;
			}
			ImGui::EndMenu();
		}
		if ( ImGui::BeginMenu( "Edit" ) )
		{
			if ( ImGui::MenuItem( "Undo", "CTRL+Z" ) ) {}
			if ( ImGui::MenuItem( "Redo", "CTRL+Y", false, false ) ) {}  // Disabled item
			ImGui::Separator();
			if ( ImGui::MenuItem( "Cut", "CTRL+X" ) ) {}
			if ( ImGui::MenuItem( "Copy", "CTRL+C" ) ) {}
			if ( ImGui::MenuItem( "Paste", "CTRL+V" ) ) {}
			ImGui::EndMenu();
		}
		ImGui::EndMainMenuBar();
	}
	ImGui::Begin( "Control Panel" );

	if ( ImGui::BeginTabBar( "Tabs" ) )
	{
		DrawSceneDebugMenu();
		DrawPostProcessDebugMenu();
		DrawAssetDebugMenu();
		DrawManipDebugMenu();
		DrawEntityDebugMenu();
		DrawDrawGroupDebugMenu();
		DeviceDebugMenu();
		DrawScheduleDebugMenu( g_renderer.GetSchedule() );

		ImGui::EndTabBar();
	}

	DrawOutlinerDebugMenu();
	DrawImageViewerDebugMenu();

	char entityName[ 256 ];
	if ( g_imguiControls.selectedEntityId >= 0 ) {
		sprintf_s( entityName, "%i: %s", g_imguiControls.selectedEntityId, ModelLib().FindName( g_scene->entities[ g_imguiControls.selectedEntityId ]->modelHdl ) );
	}
	else {
		memset( &entityName[ 0 ], 0, 256 );
	}

	ImGui::Text( "Mouse: (%f, %f)", (float)g_window.input.GetMouse().x, (float)g_window.input.GetMouse().y );
	ImGui::Text( "Mouse Dt: (%f, %f)", (float)g_window.input.GetMouse().dx, (float)g_window.input.GetMouse().dy );
	const vec4f cameraOrigin = g_scene->mainCamera->GetOrigin();
	ImGui::Text( "Camera: (%f, %f, %f)", cameraOrigin[ 0 ], cameraOrigin[ 1 ], cameraOrigin[ 2 ] );

	const vec2f ndc = g_window.GetNdc( g_window.input.GetMouse().x, g_window.input.GetMouse().y );

	ImGui::Text( "NDC: (%f, %f )", (float)ndc[ 0 ], (float)ndc[ 1 ] );
	ImGui::Text( "Frame Number: %d", g_renderDebugData.frameNumber );
	ImGui::SameLine();
	ImGui::Text( "FPS: %f", 1000.0f / g_renderDebugData.frameTimeMs );

	ImGui::End();
#endif

	scene->Update();
}


void DrawSceneDebugMenu()
{
	LogScopeSystem( "VMA" );

#if defined( USE_IMGUI )
	if ( ImGui::BeginTabItem( "Debug" ) )
	{
		g_imguiControls.rebuildShaders = ImGui::Button( "Reload Shaders" );
		//g_imguiControls.rebuildRaytraceScene = ImGui::Button( "Rebuild Raytrace Scene" );
		ImGui::SameLine();
		//g_imguiControls.raytraceScene = ImGui::Button( "Raytrace Scene" );
		//ImGui::SameLine();
		//g_imguiControls.rasterizeScene = ImGui::Button( "Rasterize Scene" );
		//ImGui::SameLine();
		g_imguiControls.captureScreenshot = ImGui::Button( "Capture ScreenShot" );
		ImGui::SameLine();
		if ( ImGui::Button( "Dump VMA Stats" ) )
		{
			LOG_SCOPE_SYSTEM( VMA );

			VmaTotalStatistics stats;
			vmaCalculateStatistics( AllocatorMemory::GetVmaAllocator(), &stats );

			VkPhysicalDeviceMemoryProperties memProps;
			vkGetPhysicalDeviceMemoryProperties( context.physicalDevice, &memProps );

			LogMsg( logSeverity_t::Info, "=== VMA Memory Statistics ===" );
			for ( uint32_t i = 0; i < memProps.memoryHeapCount; ++i )
			{
				const VmaStatistics& heap = stats.memoryHeap[ i ].statistics;
				if ( heap.blockCount == 0 ) {
					continue;
				}

				const VkMemoryHeapFlags heapFlags = memProps.memoryHeaps[ i ].flags;
				std::string heapType;
				if ( heapFlags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT ) { heapType += "DEVICE_LOCAL "; }
				if ( heapFlags & VK_MEMORY_HEAP_MULTI_INSTANCE_BIT ) { heapType += "MULTI_INSTANCE "; }
				if ( heapType.empty() ) { heapType = "HOST"; }

				std::string memTypeFlags;
				for ( uint32_t t = 0; t < memProps.memoryTypeCount; ++t )
				{
					if ( memProps.memoryTypes[ t ].heapIndex != i ) {
						continue;
					}
					const VkMemoryPropertyFlags flags = memProps.memoryTypes[ t ].propertyFlags;
					memTypeFlags += "  [Type " + std::to_string( t ) + "]:";
					if ( flags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT )  { memTypeFlags += " DEVICE_LOCAL"; }
					if ( flags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT )  { memTypeFlags += " HOST_VISIBLE"; }
					if ( flags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT ) { memTypeFlags += " HOST_COHERENT"; }
					if ( flags & VK_MEMORY_PROPERTY_HOST_CACHED_BIT )   { memTypeFlags += " HOST_CACHED"; }
					if ( flags == 0 )                                   { memTypeFlags += " (none)"; }
					memTypeFlags += "\n";
				}

				LogMsg( logSeverity_t::Info, "  Heap %u (%s) - %llu MB capacity:",
					i, heapType.c_str(), ( unsigned long long )( memProps.memoryHeaps[ i ].size / ( 1024 * 1024 ) ) );
				LogMsg( logSeverity_t::Info, "    %llu MB used / %llu MB allocated (%u allocations, %u blocks)",
					( unsigned long long )( heap.allocationBytes / ( 1024 * 1024 ) ), ( unsigned long long )( heap.blockBytes / ( 1024 * 1024 ) ),
					heap.allocationCount, heap.blockCount );
				LogMsg( logSeverity_t::Info, "%s", memTypeFlags.c_str() );
			}

			const VmaStatistics& total = stats.total.statistics;
			LogMsg( logSeverity_t::Info, "  Total: %llu MB used / %llu MB allocated (%u allocations, %u blocks)",
				( unsigned long long )( total.allocationBytes / ( 1024 * 1024 ) ), ( unsigned long long )( total.blockBytes / ( 1024 * 1024 ) ),
				total.allocationCount, total.blockCount );
			LogMsg( logSeverity_t::Info, "=============================" );
		}

		ImGui::Checkbox( "Is Textured", &g_imguiControls.isTextured );
		ImGui::Checkbox( "Use Diffuse IBL", &g_imguiControls.pbr.useDiffuseIBL );
		ImGui::Checkbox( "Use Specular IBL", &g_imguiControls.pbr.useSpecularIBL );

		const std::string selectedDebugName = GetLightingDebugModeName( static_cast<gpuDebugLightingMode_t>( g_imguiControls.pbr.debugLightingMode ) );

		if( ImGui::BeginCombo( "Debug Lighting Mode", selectedDebugName.c_str() ) )
		{
			const uint32_t modeCount = gpuDebugLightingMode_t::DEBUG_LIGHTING_COUNT;
			for( uint32_t menuIx = 0; menuIx < modeCount; ++menuIx )
			{
				const std::string debugName = GetLightingDebugModeName( static_cast<gpuDebugLightingMode_t>( menuIx ) );

				const bool selected = ( g_imguiControls.pbr.debugLightingMode == menuIx );
				if( ImGui::Selectable( debugName.c_str(), selected ) )
				{
					g_imguiControls.pbr.debugLightingMode = menuIx;
				}

				if( selected )
				{
					ImGui::SetItemDefaultFocus();
				}
			}
			ImGui::EndCombo();
		}

		//ImGui::SliderInt( "DebugLightingMode", &g_imguiControls.pbr.debugLightingMode, -1, gpuDebugLightingMode_t::DEBUG_LIGHTING_COUNT - 1 );

		ImGui::InputFloat( "Heightmap Height", &g_imguiControls.heightMapHeight, 0.1f, 1.0f );
		ImGui::SliderFloat( "Roughness Scale", &g_imguiControls.pbr.roughnessScale, 0.0f, 1.0f );
		ImGui::SliderFloat( "Roughness Bias", &g_imguiControls.pbr.roughnessBias, -1.0f, 1.0f );
		ImGui::SliderFloat( "Metalness Scale", &g_imguiControls.pbr.metalnessScale, 0.0f, 1.0f );
		ImGui::SliderFloat( "Metalness Bias", &g_imguiControls.pbr.metalnessBias, -1.0f, 1.0f );
		ImGui::SliderFloat( "Shadow Strength", &g_imguiControls.shadowStrength, 0.0f, 1.0f );

		g_gpuTimerPool.DrawDebugMenu();

		ImGui::EndTabItem();
	}
#endif
}


void DrawPostProcessDebugMenu()
{
#if defined( USE_IMGUI )
	if ( ImGui::BeginTabItem( "Post Process" ) )
	{
		postProcessControls_t& postProcess = g_imguiControls.postProcess;

		ImGui::InputFloat( "Tone Map R", &postProcess.toneMapColor[ 0 ], 0.1f, 1.0f );
		ImGui::InputFloat( "Tone Map G", &postProcess.toneMapColor[ 1 ], 0.1f, 1.0f );
		ImGui::InputFloat( "Tone Map B", &postProcess.toneMapColor[ 2 ], 0.1f, 1.0f );
		ImGui::InputFloat( "Tone Map A", &postProcess.toneMapColor[ 3 ], 0.1f, 1.0f );
		ImGui::Checkbox( "Bloom Enabled", &postProcess.bloomEnable );
		ImGui::InputFloat( "Bloom Blend Weight", &postProcess.bloomBlendWeight, 0.001f, 1.0f );
		ImGui::Checkbox( "Auto-Exposure Enabled", &postProcess.autoExposureEnable );
		ImGui::InputFloat( "Exposure Middle-Gray", &postProcess.exposureMidGray, 0.1f, 0.9f );
		ImGui::InputFloat( "Exposure Adaptation", &postProcess.exposureAdaptation, 0.01f, 5.0f );
		ImGui::InputFloat( "Exposure WhitePoint", &postProcess.exposureWhitePoint, 0.8f, 1000.0f );
		ImGui::InputFloat( "Exposure Dark Cutoff", &postProcess.exposureDarkLimit, 0.005f, 0.2f );
		ImGui::Checkbox( "Chromatic Aberration Enable", &postProcess.caEnable );
		ImGui::InputFloat( "Chromatic Aberration Intensity", &postProcess.caIntensity, 0.005f, 0.2f );
		ImGui::Checkbox( "DoF Enabled", &postProcess.dofEnable );

		ImGui::EndTabItem();
	}
#endif
}


void DrawAssetDebugMenu()
{
#if defined( USE_IMGUI )
	static ImGuiTableFlags tableFlags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg;

	if ( ImGui::BeginTabItem( "Assets" ) )
	{
		const uint32_t matCount = MaterialLib().Count();
		if ( ImGui::TreeNode( "Materials", "Materials (%i)", matCount ) )
		{
			for ( uint32_t m = 0; m < matCount; ++m )
			{
				Asset<Material>* matAsset = MaterialLib().Find( m );
				Material& mat = matAsset->Get();
				const char* matName = MaterialLib().FindName( m );

				if ( ImGui::TreeNode( matAsset->GetName().c_str() ) )
				{
					DebugMenuMaterialEdit( matAsset );
					ImGui::Separator();
					ImGui::TreePop();
				}
			}
			ImGui::TreePop();
		}
		const uint32_t modelCount = ModelLib().Count();
		if ( ImGui::TreeNode( "Models", "Models (%i)", modelCount ) )
		{
			for ( uint32_t m = 0; m < modelCount; ++m )
			{
				Asset<Model>* modelAsset = ModelLib().Find( m );
				DebugMenuModelTreeNode( modelAsset );
			}
			ImGui::TreePop();
		}
		const uint32_t texCount = ImageLib().Count();
		if ( ImGui::TreeNode( "Textures", "Textures (%i)", texCount ) )
		{
			uint64_t totalCpuBytes = 0;
			uint64_t totalGpuBytes = 0;
			for ( uint32_t t = 0; t < texCount; ++t )
			{
				const Image& tex = ImageLib().Find( t )->Get();
				if ( tex.cpuImage != nullptr ) {
					totalCpuBytes += tex.cpuImage->GetByteCount();
				}
				if ( tex.gpuImage != nullptr ) {
					totalGpuBytes += tex.gpuImage->GetByteCount();
				}
			}
			ImGui::Text( "CPU: %s", FormatByteSize( totalCpuBytes ) );
			ImGui::SameLine();
			ImGui::Text( "  |  GPU: %s", FormatByteSize( totalGpuBytes ) );
			ImGui::SameLine();
			ImGui::Text( "  |  Total: %s", FormatByteSize( totalCpuBytes + totalGpuBytes ) );
			ImGui::Separator();

			for ( uint32_t t = 0; t < texCount; ++t )
			{
				Asset<Image>* texAsset = ImageLib().Find( t );
				DebugMenuTextureTreeNode( texAsset );
			}
			ImGui::TreePop();
		}

		const uint32_t shaderCount = GpuProgramLib().Count();
		if ( ImGui::TreeNode( "Shaders", "Shaders (%i)", shaderCount ) )
		{
			std::vector<Asset<GpuProgram>*> sortedShaders( shaderCount );
			for ( uint32_t s = 0; s < shaderCount; ++s )
			{
				sortedShaders[ s ] = GpuProgramLib().Find( s );
			}
			std::sort( sortedShaders.begin(), sortedShaders.end(), []( const Asset<GpuProgram>* a, const Asset<GpuProgram>* b )
			{
				return a->GetName() < b->GetName();
			} );
			for ( Asset<GpuProgram>* shaderAsset : sortedShaders )
			{
				DebugMenuShaderTreeNode( shaderAsset );
			}
			ImGui::TreePop();
		}
		ImGui::EndTabItem();
	}
#endif
}


void DrawManipDebugMenu()
{
#if defined( USE_IMGUI )
	if ( ImGui::BeginTabItem( "Manip" ) )
	{
		static uint32_t currentIdx = 0;
		Entity* ent = g_scene->FindEntity( currentIdx );
		const char* previewValue = ent->name.c_str();
		if ( ImGui::BeginCombo( "Entity", previewValue ) )
		{
			const uint32_t modelCount = ModelLib().Count();
			for ( uint32_t e = 0; e < g_scene->EntityCount(); ++e )
			{
				Entity* comboEnt = g_scene->FindEntity( e );

				const bool selected = ( currentIdx == e );
				if ( ImGui::Selectable( comboEnt->name.c_str(), selected ) ) {
					currentIdx = e;
				}

				if ( selected ) {
					ImGui::SetItemDefaultFocus();
				}
			}
			ImGui::EndCombo();
		}
		if ( ent != nullptr )
		{
			const vec3f o = ent->GetOrigin();
			const vec3f s = ent->GetScale();
			const mat4x4f r = ent->GetRotation();
			float origin[ 3 ] = { o[ 0 ], o[ 1 ], o[ 2 ] };
			ImGui::PushItemWidth( 100 );
			ImGui::Text( "Origin" );
			ImGui::SameLine();
			ImGui::InputFloat( "##OriginX", &origin[ 0 ], 0.1f, 1.0f );
			ImGui::SameLine();
			ImGui::InputFloat( "##OriginY", &origin[ 1 ], 0.1f, 1.0f );
			ImGui::SameLine();
			ImGui::InputFloat( "##OriginZ", &origin[ 2 ], 0.1f, 1.0f );

			float scale[ 3 ] = { s[ 0 ], s[ 1 ], s[ 2 ] };
			ImGui::Text( "Scale" );
			ImGui::SameLine();
			ImGui::InputFloat( "##ScaleX", &scale[ 0 ], 0.1f, 1.0f );
			ImGui::SameLine();
			ImGui::InputFloat( "##ScaleY", &scale[ 1 ], 0.1f, 1.0f );
			ImGui::SameLine();
			ImGui::InputFloat( "##ScaleZ", &scale[ 2 ], 0.1f, 1.0f );

			float rotation[ 3 ] = { 0.0f, 0.0f, 0.0f };
			MatrixToEulerZYX( r, rotation[ 0 ], rotation[ 1 ], rotation[ 2 ] );

			ImGui::Text( "Rotation" );
			ImGui::SameLine();
			ImGui::InputFloat( "##RotationX", &rotation[ 0 ], 1.0f, 10.0f );
			ImGui::SameLine();
			ImGui::InputFloat( "##RotationY", &rotation[ 1 ], 1.0f, 10.0f );
			ImGui::SameLine();
			ImGui::InputFloat( "##RotationZ", &rotation[ 2 ], 1.0f, 10.0f );
			ImGui::PopItemWidth();

			ent->SetOrigin( origin );
			ent->SetScale( scale );
			ent->SetRotation( rotation );

			if ( ImGui::Button( "Add OBB" ) )
			{
				AABB bounds = ent->GetLocalBounds();
				const vec3f boundScale = 0.5f * ( bounds.GetMax() - bounds.GetMin() );
				const vec3f boundCenter = ( ent->GetMatrix() * vec4f( bounds.GetCenter(), 1.0f ) ).xyz;

				Entity* boundEnt = new Entity( *ent );
				boundEnt->name = ent->name + "_bounds";
				boundEnt->SetFlag( ENT_FLAG_WIREFRAME );
				boundEnt->materialHdl = MaterialLib().RetrieveHdl( "DEBUG_WIRE" );
				boundEnt->SetOrigin( boundCenter );
				boundEnt->SetScale( Multiply( vec3f( scale ), vec3f( boundScale[ 0 ], boundScale[ 1 ], boundScale[ 2 ] ) ) );
				boundEnt->SetRotation( rotation );

				g_scene->entities.push_back( boundEnt );
				g_scene->CreateEntityBounds( ModelLib().RetrieveHdl( "cube" ), *boundEnt );
			}

			ImGui::SameLine();
			if ( ImGui::Button( "Export Model" ) )
			{
				Asset<Model>* asset = ModelLib().Find( ent->modelHdl );
				WriteModel( asset, BakePath + asset->GetName() + BakedModelExtension );
			}
			ImGui::SameLine();
			bool hidden = ent->HasFlag( ENT_FLAG_NO_DRAW );
			if ( ImGui::Checkbox( "Hide", &hidden ) )
			{
				if ( hidden ) {
					ent->SetFlag( ENT_FLAG_NO_DRAW );
				}
				else {
					ent->ClearFlag( ENT_FLAG_NO_DRAW );
				}
			}
			ImGui::SameLine();
			bool wireframe = ent->HasFlag( ENT_FLAG_WIREFRAME );
			if ( ImGui::Checkbox( "Wireframe", &wireframe ) )
			{
				if ( wireframe ) {
					ent->SetFlag( ENT_FLAG_WIREFRAME );
				}
				else {
					ent->ClearFlag( ENT_FLAG_WIREFRAME );
				}
			}
		}
		ImGui::EndTabItem();
	}
#endif
}


void DrawEntityDebugMenu()
{
#if defined( USE_IMGUI )
	if ( ImGui::BeginTabItem( "Create Entity" ) )
	{
		auto MaterialCombo = []( const char* label, uint32_t& selectedIdx )
		{
			if ( ImGui::BeginCombo( label, MaterialLib().FindName( selectedIdx ) ) )
			{
				const uint32_t matCount = MaterialLib().Count();
				for ( uint32_t m = 0; m < matCount; ++m )
				{
					const bool selected = ( selectedIdx == m );
					if ( ImGui::Selectable( MaterialLib().FindName( m ), selected ) ) {
						selectedIdx = m;
					}
					if ( selected ) {
						ImGui::SetItemDefaultFocus();
					}
				}
				ImGui::EndCombo();
			}
		};

		if ( ImGui::CollapsingHeader( "From Loaded Model" ) )
		{
			static char name[ 128 ] = {};
			static uint32_t currentIdx = 0;

			if ( ImGui::BeginCombo( "Model##model", ModelLib().FindName( currentIdx ) ) )
			{
				const uint32_t modelCount = ModelLib().Count();
				for ( uint32_t m = 0; m < modelCount; ++m )
				{
					const bool selected = ( currentIdx == m );
					if ( ImGui::Selectable( ModelLib().FindName( m ), selected ) ) {
						currentIdx = m;
					}
					if ( selected ) {
						ImGui::SetItemDefaultFocus();
					}
				}
				ImGui::EndCombo();
			}
			ImGui::InputText( "Name##nameModel", name, 128 );

			if ( ImGui::Button( "Create##model" ) )
			{
				Entity* ent = new Entity();
				ent->name = name;
				ent->SetFlag( ENT_FLAG_DEBUG );
				g_scene->entities.push_back( ent );
				g_scene->CreateEntityBounds( ModelLib().RetrieveHdl( ModelLib().FindName( currentIdx ) ), *ent );
			}
		}

		if ( ImGui::CollapsingHeader( "Plane" ) )
		{
			static char planeName[ 128 ] = {};
			static uint32_t planeMatIdx = 0;
			static float planeSizeX = 1.0f;
			static float planeSizeY = 1.0f;

			MaterialCombo( "Material##plane", planeMatIdx );
			ImGui::InputText( "Name##namePlane", planeName, 128 );
			ImGui::InputFloat( "Width##plane", &planeSizeX );
			ImGui::InputFloat( "Height##plane", &planeSizeY );

			if ( ImGui::Button( "Create##plane" ) )
			{
				Model model;
				vec2f origin = vec2f( 0.0f, 0.0f );
				vec2f size   = vec2f( planeSizeX, planeSizeY );
				CreateQuadSurface2D( model, MaterialLib().FindName( planeMatIdx ), origin, size );
				ModelLib().Add( planeName, model );

				Entity* ent = new Entity();
				ent->name = planeName;
				g_scene->entities.push_back( ent );
				g_scene->CreateEntityBounds( ModelLib().RetrieveHdl( planeName ), *ent );
			}
		}

		if ( ImGui::CollapsingHeader( "Sphere" ) )
		{
			static char sphereName[ 128 ] = {};
			static uint32_t sphereMatIdx = 0;
			static float sphereRadius = 1.0f;

			MaterialCombo( "Material##sphere", sphereMatIdx );
			ImGui::InputText( "Name##nameSphere", sphereName, 128 );
			ImGui::InputFloat( "Radius##sphere", &sphereRadius );

			if ( ImGui::Button( "Create##sphere" ) )
			{
				Model model;
				CreateSphereSurface( model, MaterialLib().FindName( sphereMatIdx ), vec3f( 0.0f, 0.0f, 0.0f ), sphereRadius );
				ModelLib().Add( sphereName, model );

				Entity* ent = new Entity();
				ent->name = sphereName;
				g_scene->entities.push_back( ent );
				g_scene->CreateEntityBounds( ModelLib().RetrieveHdl( sphereName ), *ent );
			}
		}

		if ( ImGui::CollapsingHeader( "Box" ) )
		{
			static char boxName[ 128 ] = {};
			static uint32_t boxMatIdx = 0;
			static float boxSize = 1.0f;

			MaterialCombo( "Material##box", boxMatIdx );
			ImGui::InputText( "Name##nameBox", boxName, 128 );
			ImGui::InputFloat( "Size##box", &boxSize );

			if ( ImGui::Button( "Create##box" ) )
			{
				Model model;
				CreateBoxSurface( model, MaterialLib().FindName( boxMatIdx ), vec3f( 0.0f, 0.0f, 0.0f ), boxSize );
				ModelLib().Add( boxName, model );

				Entity* ent = new Entity();
				ent->name = boxName;
				g_scene->entities.push_back( ent );
				g_scene->CreateEntityBounds( ModelLib().RetrieveHdl( boxName ), *ent );
			}
		}

		if ( ImGui::CollapsingHeader( "Torus" ) )
		{
			static char torusName[ 128 ] = {};
			static uint32_t torusMatIdx = 0;
			static float torusInnerRadius = 0.5f;
			static float torusOuterRadius = 1.0f;

			MaterialCombo( "Material##torus", torusMatIdx );
			ImGui::InputText( "Name##nameTorus", torusName, 128 );
			ImGui::InputFloat( "Inner Radius##torus", &torusInnerRadius );
			ImGui::InputFloat( "Outer Radius##torus", &torusOuterRadius );

			if ( ImGui::Button( "Create##torus" ) )
			{
				Model model;
				CreateTorusSurface( model, MaterialLib().FindName( torusMatIdx ), vec3f( 0.0f, 0.0f, 0.0f ), torusInnerRadius, torusOuterRadius );
				ModelLib().Add( torusName, model );

				Entity* ent = new Entity();
				ent->name = torusName;
				g_scene->entities.push_back( ent );
				g_scene->CreateEntityBounds( ModelLib().RetrieveHdl( torusName ), *ent );
			}
		}

		if ( ImGui::CollapsingHeader( "Capsule" ) )
		{
			static char capsuleName[ 128 ] = {};
			static uint32_t capsuleMatIdx = 0;
			static float capsuleRadius = 0.5f;
			static float capsuleCylHeight = 1.0f;

			MaterialCombo( "Material##capsule", capsuleMatIdx );
			ImGui::InputText( "Name##nameCapsule", capsuleName, 128 );
			ImGui::SliderFloat( "Radius##capsule", &capsuleRadius, 0.01f, 10.0f );
			ImGui::SliderFloat( "Cylinder Height##capsule", &capsuleCylHeight, 0.0f, 20.0f );

			if ( ImGui::Button( "Create##capsule" ) )
			{
				Model model;
				CreateCapsuleSurface( model, MaterialLib().FindName( capsuleMatIdx ), vec3f( 0.0f, 0.0f, 0.0f ), capsuleRadius, capsuleCylHeight );
				ModelLib().Add( capsuleName, model );

				Entity* ent = new Entity();
				ent->name = capsuleName;
				g_scene->entities.push_back( ent );
				g_scene->CreateEntityBounds( ModelLib().RetrieveHdl( capsuleName ), *ent );
			}
		}

		if ( ImGui::CollapsingHeader( "Pyramid" ) )
		{
			static char pyramidName[ 128 ] = {};
			static uint32_t pyramidMatIdx = 0;
			static float pyramidBaseRadius = 1.0f;
			static float pyramidHeight = 2.0f;
			static int   pyramidSides = 4;

			MaterialCombo( "Material##pyramid", pyramidMatIdx );
			ImGui::InputText( "Name##namePyramid", pyramidName, 128 );
			ImGui::SliderFloat( "Base Radius##pyramid", &pyramidBaseRadius, 0.01f, 10.0f );
			ImGui::SliderFloat( "Height##pyramid", &pyramidHeight, 0.01f, 20.0f );
			ImGui::SliderInt( "Sides##pyramid", &pyramidSides, 3, 16 );

			if ( ImGui::Button( "Create##pyramid" ) )
			{
				Model model;
				CreatePyramidSurface( model, MaterialLib().FindName( pyramidMatIdx ), vec3f( 0.0f, 0.0f, 0.0f ), pyramidBaseRadius, pyramidHeight, static_cast<uint32_t>( pyramidSides ) );
				ModelLib().Add( pyramidName, model );

				Entity* ent = new Entity();
				ent->name = pyramidName;
				g_scene->entities.push_back( ent );
				g_scene->CreateEntityBounds( ModelLib().RetrieveHdl( pyramidName ), *ent );
			}
		}

		if ( ImGui::CollapsingHeader( "Diamond" ) )
		{
			static char diamondName[ 128 ] = {};
			static uint32_t diamondMatIdx = 0;
			static float diamondRadius = 1.0f;
			static float diamondTopHeight = 1.0f;
			static float diamondBotHeight = 0.5f;
			static int   diamondSides = 6;

			MaterialCombo( "Material##diamond", diamondMatIdx );
			ImGui::InputText( "Name##nameDiamond", diamondName, 128 );
			ImGui::SliderFloat( "Radius##diamond", &diamondRadius, 0.01f, 10.0f );
			ImGui::SliderFloat( "Top Height##diamond", &diamondTopHeight, 0.01f, 20.0f );
			ImGui::SliderFloat( "Bottom Height##diamond", &diamondBotHeight, 0.01f, 20.0f );
			ImGui::SliderInt( "Sides##diamond", &diamondSides, 3, 16 );

			if ( ImGui::Button( "Create##diamond" ) )
			{
				Model model;
				CreateDiamondSurface( model, MaterialLib().FindName( diamondMatIdx ), vec3f( 0.0f, 0.0f, 0.0f ), diamondRadius, diamondTopHeight, diamondBotHeight, static_cast<uint32_t>( diamondSides ) );
				ModelLib().Add( diamondName, model );

				Entity* ent = new Entity();
				ent->name = diamondName;
				g_scene->entities.push_back( ent );
				g_scene->CreateEntityBounds( ModelLib().RetrieveHdl( diamondName ), *ent );
			}
		}

		ImGui::EndTabItem();
	}
#endif
}



void DrawDrawGroupDebugMenu()
{
#if defined( USE_IMGUI )
	if ( ImGui::BeginTabItem( "Draw Groups" ) )
	{
		ImGui::EndTabItem();
	}
#endif
}


void DrawOutlinerDebugMenu()
{
#if defined( USE_IMGUI )
	ImGui::Begin( "Outliner" );

	static ImGuiTableFlags flags = ImGuiTableFlags_BordersV | ImGuiTableFlags_BordersOuterH | ImGuiTableFlags_Resizable | ImGuiTableFlags_RowBg | ImGuiTableFlags_NoBordersInBody;

	DebugMenuEntityEdit( g_scene );

	ImGui::End();
#endif
}
