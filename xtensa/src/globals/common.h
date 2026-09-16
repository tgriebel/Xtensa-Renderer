#pragma once

#if defined( RENDERER )
	#define USE_VULKAN
	#define USE_VULKAN_RTX
	#define USE_IMGUI
	#define USE_GLFW
	#define USE_TINYFD
#elif defined( BAKER )
	// no GPU API features needed for the baker
#endif

#if defined( USE_GLFW )
	#define GLFW_INCLUDE_VULKAN
#endif

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#undef near
#undef far

#if defined( USE_GLFW )
	#include <GLFW/glfw3.h>
#endif


#include <iostream>
#include <stdexcept>
#include <cstdlib>
#include <vector>
#include <utility>
#include <set>
#include <cstdint>
#include <algorithm>
#include <fstream>
#include <array>
#include <thread>
#include <chrono>
#include <ctime>
#include <ratio>
#include <unordered_map>
#include <stdlib.h>
#include <math.h>
#include <atomic>

#include <GfxCore/math/vector.h>
#include <GfxCore/image/color.h>
#include <GfxCore/primitives/geom.h>
#include <GfxCore/acceleration/aabb.h>

#include <SysCore/handle.h>
#include <SysCore/common.h>
#include <SysCore/array.h>
#include <SysCore/timer.h>

#include "../asset_types/material.h"
#include "../scene/camera.h"
#include "../render_core/log.h"

const uint32_t	DescriptorPoolMaxUniformBuffers	= 1024;
const uint32_t	DescriptorPoolMaxStorageBuffers	= 1024;
const uint32_t	DescriptorPoolMaxSamplers		= 16;
const uint32_t	DescriptorPoolMaxImages			= 8192; // Must account for bindless arrays (MaxImageDescriptors) * MaxFrameStates * num bind set instances
const uint32_t	DescriptorPoolMaxStorageImages	= 64;
const uint32_t	DescriptorPoolMaxComboImages	= 128;
#ifdef USE_VULKAN_RTX
const uint32_t	DescriptorPoolMaxAccelStructures = 16; // One per RT bind set instance
#endif
const uint32_t	DescriptorPoolMaxSets			= ( DescriptorPoolMaxUniformBuffers + DescriptorPoolMaxStorageBuffers + \
													DescriptorPoolMaxImages + DescriptorPoolMaxComboImages + DescriptorPoolMaxSamplers );
const uint32_t	MaxImageDescriptors				= 128;
const uint32_t	MaxParticles					= 1024;
const uint32_t	MaxShadowMaps					= 6;
const uint32_t	MaxShadowViews					= MaxShadowMaps; // TODO: No max for specific view types
const uint32_t	MaxFrameImages					= 32;
const uint32_t	MaxMipMaps						= 16;
const uint32_t	Max2DViews						= 2;			// TODO: No max for specific view types
const uint32_t	Max3DViews						= 7;			// TODO: No max for specific view types
const uint32_t	MaxModels						= 1024;
const uint32_t	MaxVertices						= 0x00FFFFFF;
const uint32_t	MaxIndices						= 0x00FFFFFF;
const uint32_t	MaxSurfacesDescriptors			= 1;
const uint32_t	MaxCodeImages					= 8;
const uint64_t	MaxSharedMemory					= MB( 1024 );
const uint64_t	MaxLocalMemory					= MB( 1024 );
const uint64_t	MaxScratchMemory				= MB( 64 );
const uint64_t	MaxFrameBufferMemory			= GB( 2 );
const uint64_t	MaxTexturingUploadMemory		= MB( 600 );
const uint64_t	MaxGeometryUploadMemory			= MB( 128 );
const uint32_t	MaxFrameStates					= 3;
const uint64_t	MaxTimeStampQueries				= 16;
const uint64_t	MaxOcclusionQueries				= 16;
const uint32_t	DefaultDisplayWidth				= 1280;
const uint32_t	DefaultDisplayHeight			= 720;
const bool		ForceDisableMSAA				= false;

const std::string ModelPath = ".\\assets\\";
const std::string TexturePath = ".\\assets\\";
const std::string CodeAssetPath = ".\\code_assets\\";
const std::string ScreenshotPath = "..\\screenshots\\";
const std::string ScenePath = ".\\scenes\\";
const std::string MaterialPath = ".\\materials\\";
const std::string BakePath = ".\\baked\\";
const std::string BakedModelExtension = ".mdl.bin";
const std::string BakedTextureExtension = ".img.bin";
const std::string BakedMaterialExtension = ".mtl.bin";

uint32_t Hash( const uint8_t* bytes, const uint32_t sizeBytes );

#define THROW_ERROR( MESSAGE ) \
	do { \
		const std::string throwErrorMsg_( MESSAGE ); \
		LogMsg( logSeverity_t::Error, "%s", throwErrorMsg_.c_str() ); \
		throw std::runtime_error( throwErrorMsg_ ); \
	} while( false ) // do-while(0) is a common idiom to ensure the macro behaves like a single statement

#define FATAL_ERROR( MESSAGE ) THROW_ERROR( "Fatal Error: " #MESSAGE )

typedef void ( *debugMenuFuncPtr )( );

class Renderer;
class Serializer;
class Image;
class GpuProgram;

template<class AssetType>
class Asset;

enum renderFlags_t : uint32_t
{
	NONE		= 0,
	HIDDEN		= ( 1 << 0 ),
	NO_SHADOWS	= ( 1 << 1 ),
	WIREFRAME	= ( 1 << 2 ),	// FIXME: this needs to make a unique pipeline object if checked
	DEBUG_SOLID	= ( 1 << 3 ),	// "
	SKIP_OPAQUE	= ( 1 << 4 ),
	COMMITTED	= ( 1 << 5 ),
};


enum class swapBuffering_t : uint8_t
{
	SINGLE_FRAME,
	MULTI_FRAME,
};
