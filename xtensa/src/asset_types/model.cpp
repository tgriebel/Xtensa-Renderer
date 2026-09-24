#include "model.h"
#include <syscore/systemUtils.h>
#include <syscore/serializer.h>
#include <gfxcore/io/serializeClasses.h>
#include "material.h"
#include "../scene/assetManager.h"
#include "../scene/assetBaker.h"
#include "../io/serializeClasses.h"
#include <SysCore/log.h>

bool ModelLoader::Load( Asset<Model>& modelAsset )
{
	Model& model = modelAsset.Get();

	const std::string fileName = m_modelName + "." + m_modelExt;

	sourceFile_t modelSource {};
	modelSource.path = m_modelPath + fileName;
	modelSource.name = m_modelName;

	bakedAssetInfo_t modelInfo = {};
	const bool loadedBakedModel = LoadBaked( modelAsset, modelInfo, modelSource, ".\\baked\\" + m_modelPath, "mdl.bin" );
	if ( loadedBakedModel )
	{
		const uint32_t surfCount = static_cast<uint32_t>( model.surfs.size() );
		for ( uint32_t surfIx = 0; surfIx < surfCount; ++surfIx ) {
			assets->GetLib<Material>()->AddDeferred( model.surfs[ surfIx ].materialHdl, pMatLoader_t( new BakedMaterialLoader( assets, ".\\materials\\", "mtl.bin" ) ) );
		}
		return true;
	}

	LogMsg( "Asset", logSeverity_t::Info, "Loading raw model: %s", fileName.c_str() );

	if ( m_modelExt == "obj" ) {
		return LoadRawModelModelObj( *assets, fileName, m_modelPath, m_texturePath, model );
	} else if ( m_modelExt == "gltf" || m_modelExt == "glb" ) {
		return LoadRawModelGLTF( *assets, fileName, m_modelPath, m_texturePath, model );
	} else {
		return false;
	}
}


void ModelLoader::SetTexturePath( const std::string& path )
{
	m_texturePath = path;
}


void ModelLoader::SetModelPath( const std::string& path )
{
	m_modelPath = path;
}


void ModelLoader::SetModelName( const std::string& fileName )
{
	SysCore::SplitFileName( fileName, m_modelName, m_modelExt );
}


void ModelLoader::SetAssetRef( AssetManager* assetsPtr )
{
	assets = assetsPtr;
}
