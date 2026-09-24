#include "../../stdafx.h"

#include <thread>
#include "../globals/common.h"
#include "../globals/assetDefs.h"
#include <syscore/systemUtils.h>
#include "../scene/sceneBase.h"
#include "window.h"
#include "../globals/renderConstants.h"
#include "../render_core/renderer.h"
#include "../render_core/schedule.h"
#include "../scene/sceneParser.h"
#include "../scene/assetBaker.h"
#include "../scene/codeAssets.h"
#include "cvar.h"
#include <SysCore/log.h>

#include "../../scenes/chess/chessScene.h"
#include "../../scenes/emu/nesScene.h"

#include "imguiInterface.h"

AssetManager						g_assets;
Scene*								g_scene;
Renderer							g_renderer;
Window								g_window;

using namespace SysCore;


static const char* sceneFile = "chess";

#if defined( USE_IMGUI )
imguiControls_t g_imguiControls;
#endif

void UpdateScene( Scene* scene );
void InitScene( Scene* scene );
void ShutdownScene( Scene* scene );


void RenderThread()
{
}


void CheckReloadAssets()
{
#if defined( USE_IMGUI )
	if ( g_imguiControls.rebuildShaders )
	{
		GpuProgramLib().UnloadAll();
		GpuProgramLib().LoadAll( true );

		g_imguiControls.rebuildShaders = false;
	}

	if( g_imguiControls.shaderHdl != INVALID_HDL )
	{
		Asset<GpuProgram>* prog = GpuProgramLib().Find( g_imguiControls.shaderHdl );
		prog->Reload( true );
		g_imguiControls.shaderHdl = INVALID_HDL;
	}
#endif
}


MakeCVar( BOOL,		r_writeCubeCapture, false );
MakeCVar( BOOL,		r_computeEnvMap, false );
MakeCVar( BOOL,		r_computeDiffuseIbl, false );
MakeCVar( BOOL,		r_computeSpecularIbl, false );
MakeCVar( BOOL,		r_computeBrdfLut, false );
MakeCVar( BOOL,		r_computeNoiseImage, false );
MakeCVar( BOOL,		r_exitAfterJobComplete, false );
MakeCVar( STRING,	c_scene, sceneFile );
MakeCVar( STRING,	r_hdrCubemapSource, "" );
MakeCVar( STRING,	r_cubemapName, "chess" );
MakeCVar( BOOL,		c_bakeAssets, false );
MakeCVar( BOOL,		c_loadBakedAssets, true );
MakeCVar( BOOL,		s_threadedLoad, true );
MakeCVar( BOOL,		r_shadows, true );
MakeCVar( BOOL,		r_downsampleScene, true );
MakeCVar( BOOL,		r_bloom, true );
MakeCVar( BOOL,		r_chromaticAberration, true );
MakeCVar( BOOL,		r_autoExposure, true );
MakeCVar( BOOL,		r_screenshot, true );
MakeCVar( BOOL,		r_gaussianBlur, true );
MakeCVar( BOOL,		r_ssao, true );
MakeCVar( BOOL,		r_dof, true );
MakeCVar( BOOL,		r_rtReflection, true );
MakeCVar( BOOL,		c_imgui, true );
MakeCVar( INT,		r_fullscreenMode, 0 );
MakeCVar( INT,		r_windowWidth, -1 );
MakeCVar( INT,		r_windowHeight, -1 );
MakeCVar( INT,		r_cubeWidth, 1024 );
MakeCVar( INT,		r_cubeHeight, 1024 );
 

void ParseCmdArgs( const int argc, char* argv[] )
{
	for ( int32_t i = 1; i < argc; ++i ) {
		CVar::ParseCommand( argv[ i ] );
	}
}


void ParseConfig( const std::string& fileName )
{
	std::ifstream file;

	file.open( fileName );

	if ( !file.is_open() ) {
		THROW_ERROR( "Failed to open config file!" );
	}

	while ( file.good() )
	{
		std::string line;
		getline( file, line );

		CVar::ParseCommand( line );
	}
	file.close();
}


// `LocalConfig.ini` overrides `DefaultConfig.ini`.
// Command line arguments override both config files.
void LoadConfigs( const int argc, char* argv[] )
{
	bool hasCmdLineConfig = false;
	for( int32_t i = 1; i < argc; ++i )
	{
		if( HasSuffix( argv[ i ], ".ini" ) )
		{
			hasCmdLineConfig = true;
			break;
		}
	}

	if( hasCmdLineConfig == false )
	{
		ParseConfig( "DefaultConfig.ini" );

		const std::string localConfigFile = "LocalConfig.ini";
		if ( SysCore::FileExists( localConfigFile ) == false ) {
			SysCore::CloneFile( "DefaultConfig.ini", localConfigFile );
		}
		ParseConfig( localConfigFile );
	}

	for( int32_t i = 1; i < argc; ++i )
	{
		if( HasSuffix( argv[ i ], ".ini" ) )
		{
			std::string fileName = argv[ i ];
			ParseConfig( fileName );
		}
		else
		{
			CVar::ParseCommand( argv[ i ] );
		}
	}
}


void InitSceneType( const std::string type, Scene** scene )
{
	if ( type == "chess" ) {
		*scene = new ChessScene();
	}
	else if ( type == "nes" ) {
		*scene = new NesScene();
	}
	else {
		*scene = new Scene();
	}
}


void CubeCaptureLoad( Scene* scene )
{
	g_assets.GetLib<Image>()->AddDeferred( r_hdrCubemapSource.GetString(), pImgLoader_t( new ImageLoader( TexturePath, r_hdrCubemapSource.GetString(), false ) ) );

	Material& material = g_assets.GetLib<Material>()->Find( "_sky" )->Get();

	const hdl_t cubemapTextureHdl = AssetLib<GpuProgram>::Handle( r_hdrCubemapSource.GetString() );

	const bool sampleAsCubemap = false;
	if( sampleAsCubemap ) {
		material.AddShader( DRAWPASS_SKYBOX, AssetLib<GpuProgram>::Handle( "Sky" ), (uint32_t)shaderPermId_t::SKY_CUBE_SAMPLER );
	} else {
		material.AddShader( DRAWPASS_SKYBOX, AssetLib<GpuProgram>::Handle( "EquirectangularSampler" ) );	
	}
	material.AddTexture( 0, cubemapTextureHdl );
}


int main( int argc, char* argv[] )
{
	g_assets.RegisterLib<Model>( "Model" );
	g_assets.RegisterLib<Image>( "Image" );
	g_assets.RegisterLib<Material>( "Material" );
	g_assets.RegisterLib<GpuProgram>( "Gpu Program" );

	CreateCodeAssets(); // TODO: Check render dependencies, may need to move into render init?

	LoadConfigs( argc, argv );

	if ( c_bakeAssets.GetBool() || c_loadBakedAssets.GetBool() == false ) {
		ToggleBakedLoading( false );
	}

	if( c_scene.IsValid() ) {
		LoadScene( SceneJsonPath( c_scene.GetString() ), &g_scene, &g_assets, InitSceneType );
	} else {
		LoadScene( SceneJsonPath( sceneFile ), &g_scene, &g_assets, InitSceneType );
	}

	renderConfig_t config {};
	config.useCubeViews = r_computeEnvMap.GetBool() || r_computeDiffuseIbl.GetBool() || r_computeSpecularIbl.GetBool();
	config.writeCubeViews = r_writeCubeCapture.GetBool();
	config.computeEnvMap = r_computeEnvMap.GetBool();
	config.computeDiffuseIbl = r_computeDiffuseIbl.GetBool();
	config.computeSpecularIBL = r_computeSpecularIbl.GetBool();
	config.cubeDownsample = r_computeEnvMap.GetBool() || r_computeDiffuseIbl.GetBool() || r_computeSpecularIbl.GetBool();
	config.shadows = r_shadows.GetBool();
	config.downsampleScene = r_downsampleScene.GetBool();
	config.bloom = r_bloom.GetBool();
	config.chromaticAberration = r_chromaticAberration.GetBool();
	config.autoExposure = r_autoExposure.GetBool();
	config.screenshot = r_screenshot.GetBool();
	config.cubemapName = r_cubemapName.GetString();
	config.computeBrdfLut = r_computeBrdfLut.GetBool();
	config.computeNoiseImage = r_computeNoiseImage.GetBool();
	config.gaussianBlur = r_gaussianBlur.GetBool();
	config.ssao = r_ssao.GetBool();
	config.dof = r_dof.GetBool();
	config.rtReflections = r_rtReflection.GetBool();
	config.rayTracingEnabled = config.rtReflections;
	config.useImgui = c_imgui.GetBool();

	std::thread renderThread( RenderThread );

	InitScene( g_scene );

	if( c_bakeAssets.GetBool() )
	{
		BakeAssets();
		exit( 0 );
	}

	const bool precomputeSkycube = ( _stricmp( r_hdrCubemapSource.GetString(), "" ) != 0 );
	if( precomputeSkycube )
	{
		CubeCaptureLoad( g_scene );
		g_assets.RunLoadLoop();
	}

	g_window.Init();

	try
	{
		TaskSchedule* schedule = new TaskSchedule();

		g_renderer.Init( config );
		g_renderer.BuildSchedule( schedule );
		g_renderer.SetSchedule( schedule );

		while ( g_window.IsOpen() )
		{
			CheckReloadAssets();

			g_window.PumpMessages();

			if( g_window.IsResizeRequested() )
			{
				g_renderer.Resize();
				g_window.CompleteImageResize();
			}

#if defined( USE_IMGUI )
			if ( g_imguiControls.openModelImportFileDialog )
			{
				std::vector<const char*> filters;
				filters.push_back( "*.obj" );
				std::string path = g_window.OpenFileDialog( "Import Obj", filters, "Model files (*.obj)" );
				std::string dir;
				std::string file;

				SplitPath( path, dir, file );

				std::string modelName = path;

				ModelLoader* loader = new ModelLoader();
				loader->SetModelPath( dir );
				loader->SetTexturePath( dir );
				loader->SetModelName( file );
				loader->SetAssetRef( &g_assets );

				const hdl_t modelHdl = ModelLib().AddDeferred( file.c_str(), loader_t( loader ) );

				Entity* ent = new Entity();
				ent->name = file;
				

				g_assets.RunLoadLoop();
				//ent->materialHdl = 
				//ent->SetFlag( ENT_FLAG_DEBUG );
				g_scene->entities.push_back( ent );
				g_scene->CreateEntityBounds( modelHdl, *ent );

				g_imguiControls.openModelImportFileDialog = false;
			}

			if ( g_imguiControls.openSceneFileDialog )
			{
				std::vector<const char*> filters;
				filters.push_back( "*.json" );
				std::string path = g_window.OpenFileDialog( "Open Scene", filters, "Scene files" );

				const std::string file = SysCore::MakeRelative( path, ScenePath );

				ShutdownScene( g_scene );
				delete g_scene;
				g_scene = nullptr;
				g_renderer.ShutdownGPU();
				g_assets.Clear();

				CreateCodeAssets();
				LoadScene( file, &g_scene, &g_assets, InitSceneType );
				InitScene( g_scene );
		
				g_renderer.InitGPU();

				g_imguiControls.openSceneFileDialog = false;
			}

			if( g_imguiControls.reloadScene )
			{
				g_imguiControls.reloadScene = true;
			}
#endif

			UpdateScene( g_scene );

#if defined( USE_IMGUI )
			if ( g_imguiControls.rebuildRaytraceScene ) {
			//	BuildRayTraceScene( g_scene );
			}

			if ( g_imguiControls.raytraceScene ) {
			//	TraceScene( false );
			}

			if ( g_imguiControls.rasterizeScene ) {
			//	TraceScene( true );
			}
#endif
			
			g_window.BeginFrame();

			g_renderer.Commit( g_scene );
			g_renderer.Render();

			g_scene->AdvanceFrame();
			g_window.EndFrame();

			if( precomputeSkycube && r_exitAfterJobComplete.GetBool() ) {
				break;
			}
		}
		g_renderer.Shutdown();
		delete schedule;
	}
	catch (const std::exception& e)
	{
		LogMsg( "App", logSeverity_t::Error, "%s", e.what() );
		renderThread.join();
		return EXIT_FAILURE;
	}
	renderThread.join();
	return EXIT_SUCCESS;
}
