#pragma once

#include "../scene/assetManager.h"
#include "../asset_types/image.h"
#include "../asset_types/material.h"
#include "../asset_types/gpuProgram.h"
#include "../asset_types/model.h"
#include "../asset_types/binaryFile.h"
#include "../asset_types/assetLib.h"

typedef AssetLib< Model >			AssetLibModels;
typedef AssetLib< Image >			AssetLibImages;
typedef AssetLib< Material >		AssetLibMaterials;
typedef AssetLib< GpuProgram >		AssetLibGpuProgram;
typedef AssetLib< BinaryFile >		AssetLibBinaryFiles;

extern AssetManager g_assets;

inline AssetLib<Model>&			ModelLib()			{ return *g_assets.GetLib<Model>(); }
inline AssetLib<Image>&			ImageLib()			{ return *g_assets.GetLib<Image>(); }
inline AssetLib<Material>&		MaterialLib()		{ return *g_assets.GetLib<Material>(); }
inline AssetLib<GpuProgram>&	GpuProgramLib()		{ return *g_assets.GetLib<GpuProgram>(); }
inline AssetLib<BinaryFile>&	BinaryFileLib()		{ return *g_assets.GetLib<BinaryFile>(); }
