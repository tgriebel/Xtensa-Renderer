#include "nesScene.h"
#include "../../src/app/window.h"
#include "../../src/globals/assetDefs.h"
#include <SysCore/log.h>

#include <windows.h> 
#include <stdio.h>

#define IMPORT_WIN
#include "tomtendoCore.h"

//using namespace Tomtendo;

static const uint32_t EmuInstances = 4;
static uint64_t lastFrame[ EmuInstances ] = {};
static bool paused[ EmuInstances ] = { false, true, true, true };
static const char* textureBuffers[ EmuInstances ] = { "CODE_COLOR_0", "CODE_COLOR_1", "CODE_COLOR_2", "CODE_COLOR_3" };
static Tomtendo::Emulator* nes[ EmuInstances ];
static Tomtendo::config_t nesCfg;

static bool emulatorRunning = true;
static std::chrono::nanoseconds GlobalDelta;
static std::vector< std::thread > threadPool;

BOOL fRunTimeLinkSuccess = FALSE;
HINSTANCE hinstLib = nullptr;

Tomtendo::runtimeDllInterface_t tomtendo{};

struct bind_t
{
	key_t					key;
	Tomtendo::ButtonFlags	btn;
};

static const uint32_t ButtonCount = 8;

static bind_t ControllerBinds[ ButtonCount ] =
{
	bind_t{ KEY_H, Tomtendo::ButtonFlags::BUTTON_START },
	bind_t{ KEY_G, Tomtendo::ButtonFlags::BUTTON_SELECT },
	bind_t{ KEY_J, Tomtendo::ButtonFlags::BUTTON_B },
	bind_t{ KEY_K, Tomtendo::ButtonFlags::BUTTON_A },
	bind_t{ KEY_A, Tomtendo::ButtonFlags::BUTTON_LEFT },
	bind_t{ KEY_D, Tomtendo::ButtonFlags::BUTTON_RIGHT },
	bind_t{ KEY_W, Tomtendo::ButtonFlags::BUTTON_UP },
	bind_t{ KEY_S, Tomtendo::ButtonFlags::BUTTON_DOWN },
};

extern Window g_window;

void CopyFrameBuffer( Tomtendo::wtFrameResult& fr, hdl_t texHandle )
{
	Asset<Image>* imageAsset = ImageLib().Find( texHandle );
	Image& texture = imageAsset->Get();

	const uint32_t width = fr.frameBuffer->GetWidth();
	const uint32_t height = fr.frameBuffer->GetHeight();

	ImageBuffer<rgba8_t>* imageBuffer = reinterpret_cast<ImageBuffer<rgba8_t>*>( texture.cpuImage );

	for ( uint32_t y = 0; y < height; ++y )
	{
		for ( uint32_t x = 0; x < width; ++x )
		{
			const uint32_t pixelIx = y * width + x;
			Tomtendo::Pixel pixel = fr.frameBuffer->Get( pixelIx );

			rgba8_t rgba {};
			rgba.r = pixel.rgba.alpha;
			rgba.g = pixel.rgba.blue;
			rgba.b = pixel.rgba.green;
			rgba.a = pixel.rgba.red;

			imageBuffer->SetPixel( x, y, rgba );
		}
	}
	imageAsset->RefreshUpload();
}

static inline void ThreadRun( Tomtendo::Emulator* emu, const char* bufferName )
{
	uint64_t lastFrame = 0;
	while ( true )
	{
		if ( !tomtendo.RunEpoch( emu, GlobalDelta ) ) {
			break;
		}

		if( emulatorRunning == false ) {
			break;
		}

		for ( uint32_t k = 0; k < ButtonCount; ++k )
		{
			if ( g_window.input.IsKeyPressed( ControllerBinds[ k ].key ) ) {
				tomtendo.StoreKey( &emu->input, ControllerBinds[ k ].key );
			}
			else {
				tomtendo.ReleaseKey( &emu->input, ControllerBinds[ k ].key );
			}
		}

		Tomtendo::wtFrameResult fr;
		tomtendo.GetFrameResult( emu, fr );
		if ( fr.currentFrame > lastFrame )
		{
			lastFrame = fr.currentFrame;
			CopyFrameBuffer( fr, AssetLibImages::Handle( bufferName ) );
		}
	}
}


void NesScene::Init()
{
	// Get a handle to the DLL module.
	hinstLib = LoadLibrary( TEXT( "scenes/emu/wintendoCore.dll" ) );

	if ( hinstLib == nullptr ) {
		return;
	}
	fRunTimeLinkSuccess = TRUE;

	std::wstring filePaths[ EmuInstances ] =
	{
		L"scenes/emu/roms/Super Mario Bros.nes",
		L"scenes/emu/roms/Super C.nes",
		L"scenes/emu/roms/Ninja Gaiden.nes",
		L"scenes/emu/roms/Metroid.nes"
	};

	Tomtendo::LoadDllInterface( &tomtendo, hinstLib );

	LogMsg( "Scene", logSeverity_t::Info, "NES screen height: %u", tomtendo.ScreenHeight() );

	nesCfg = tomtendo.DefaultConfig();

	for( uint32_t i = 0; i < EmuInstances; ++i )
	{
		tomtendo.CreateEmulatorInstance( &nes[ i ] );

		tomtendo.Boot( nes[ i ], filePaths[ i ].c_str(), 0x10000 );
		tomtendo.SetConfig( nes[ i ], nesCfg );

		for ( uint32_t k = 0; k < ButtonCount; ++k )
		{
			using namespace Tomtendo;
			tomtendo.BindKey( &nes[ i ]->input, ControllerBinds[ k ].key, ControllerId::CONTROLLER_0, ControllerBinds[ k ].btn );
		}
	}

	threadPool.reserve( EmuInstances );

	for ( uint32_t i = 0; i < EmuInstances; ++i ) {
		threadPool.push_back( std::thread( ThreadRun, nes[ i ], textureBuffers[ i ] ) );
	}
}


void NesScene::Shutdown()
{
	emulatorRunning = false;
	for ( auto& thread : threadPool ) {
		thread.join();
	}

	if( fRunTimeLinkSuccess == FALSE ) {
		return;
	}

	for ( uint32_t i = 0; i < EmuInstances; ++i )
	{
		tomtendo.DestroyEmulatorInstance( &nes[ i ] );
	}
	BOOL fFreeResult = FreeLibrary( hinstLib );
}

void NesScene::Update()
{
	if( emulatorRunning ) {
		GlobalDelta = DeltaNano();
	}
}