#pragma once

#include <cinttypes>
#include <string>

#define SHADER_STRUCTS_CPP
#include "../../shaders/gpuShared.h"

class Scene;
class Model;
class Material;
class Image;
class GpuProgram;

template<class AssetType>
class Asset;

struct renderDebugData_t
{
	uint64_t	frameNumber;
	float		frameTimeMs;
	float		mouseX;
	float		mouseY;
};

extern renderDebugData_t g_renderDebugData;

const char* FormatByteSize( const uint64_t bytes );

#if defined( USE_IMGUI )
void DebugMenuMaterial( const Material& mat );
void DebugMenuMaterialEdit( Asset<Material>* matAsset );
std::string GetShaderTextureName( const Material& material, const uint32_t textureSlot );
std::string GetLightingDebugModeName( const gpuDebugLightingMode_t mode );
void DebugMenuModelTreeNode( Asset<Model>* modelAsset );
void DebugMenuTextureTreeNode( Asset<Image>* texAsset );
void DebugMenuShaderTreeNode( Asset<GpuProgram>* shaderAsset );
void DebugMenuEntityEdit( Scene* scene );
#ifdef USE_VULKAN
void DebugMenuDeviceProperties( VkPhysicalDeviceProperties deviceProperties, VkPhysicalDeviceFeatures2 deviceFeatures );
#endif
void DeviceDebugMenu();
void DrawImageViewerDebugMenu();
#endif
