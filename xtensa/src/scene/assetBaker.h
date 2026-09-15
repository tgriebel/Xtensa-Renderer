#pragma once
#include <vector>
#include <string>
#include <filesystem>
#include <chrono>

#include <SysCore/common.h>
#include <SysCore/serializer.h>
#include <SysCore/systemUtils.h>

#include "../asset_types/assetLib.h"
#include "../render_core/log.h"

class Model;
class Material;
class Image;
class GpuProgram;

struct bakedAssetInfo_t
{
	std::string		name;
	std::string		hash;
	std::string		type;
	std::string		date;		// Unix epoch seconds as string
	uint32_t		sizeBytes;
	uint64_t		dataHash;
};

struct sourceFile_t
{
	std::string		path;
	std::string		name;
	std::string		type;
	bool			isBakedAsset;
};

void ToggleBakedLoading( const bool enabled );

bool AreBakedAssetsEnabled();

bool IsBakedAssetFresh( const sourceFile_t& source, const bakedAssetInfo_t& bakedInfo );

void BakeAssets();

class AssetBaker
{
private:
	std::string				m_bakePath;
	std::string				m_modelPath;	
	std::string				m_modelExt;
	std::string				m_materialPath;
	std::string				m_materialExt;
	std::string				m_imagePath;
	std::string				m_imageExt;
	AssetLib<Model>*		m_modelLib;
	AssetLib<Material>*		m_materialLib;
	AssetLib<Image>*		m_imageLib;
public:
	AssetBaker() {}

	void AddAssetLib( AssetLib<Model>* lib, const std::string path, const std::string ext );
	void AddAssetLib( AssetLib<Material>* lib, const std::string path, const std::string ext );
	void AddAssetLib( AssetLib<Image>* lib, const std::string path, const std::string ext );
	void AddBakeDirectory( const std::string path );
	void Bake();
};


template<class T>
static bool StoreBaked( Asset<T>& asset, bakedAssetInfo_t& info, const std::string& path, const std::string& ext )
{
	if( asset.CanBake() == false ) {
		return false;
	}

	Serializer* s = new Serializer( MB( 128 ), serializeMode_t::STORE );

	info.name = asset.GetName();
	info.hash = asset.Handle().String();

	s->SetPosition( 0 );
	s->NextString( info.name );
	s->NextString( info.type );
	s->NextString( info.date );

	asset.Serialize( s );

	info.sizeBytes = s->CurrentSize();

	uint32_t byteCount = info.sizeBytes;
	uint64_t dataHash = s->Hash();
	s->Next( byteCount );
	s->Next( dataHash );

	info.dataHash = dataHash;

	s->WriteFile( path + asset.Handle().String() + ext );

	delete s;

	return true;
}


template<class T>
bool LoadBaked( Asset<T>& asset, bakedAssetInfo_t& info, const sourceFile_t& source, const std::string& dir, const std::string& ext )
{
	LOG_SCOPE_SYSTEM( Asset );

	if( AreBakedAssetsEnabled() == false ) {
		return false;
	}

	const hdl_t handle = asset.Handle();
	const std::string hash = handle.String();
	const std::string bakedPath = dir + hash + "." + ext;

	if( SysCore::FileExists( bakedPath ) )
	{
		const uint32_t fileSize = static_cast< uint32_t >( std::filesystem::file_size( bakedPath ) );
		Serializer s( fileSize, serializeMode_t::LOAD );
		s.ReadFile( bakedPath );

		s.SetPosition( 0 );
		s.NextString( info.name );
		s.NextString( info.type );
		s.NextString( info.date );

		if( IsBakedAssetFresh( source, info ) == false )
		{
			LogMsg( "Baked file out-of-date: %s source is newer.", info.name.c_str() );
			return false;
		}

		asset.Serialize( &s );

		assert( info.name.length() > 0 );

		asset.Rename( info.name );

		info.sizeBytes = s.CurrentSize();
		info.hash = Library::Handle( info.name.c_str() ).String();

		const uint64_t currentHash = s.Hash();

		uint32_t byteCount;
		uint64_t dataHash;
		s.Next( byteCount );
		s.Next( dataHash );

		if( currentHash != dataHash )
		{
			LogMsg( logSeverity_t::Warning, "Baked hash mismatch: %llu != %llu", static_cast<unsigned long long>( currentHash ), static_cast<unsigned long long>( dataHash ) );
			return false;
		}

		if( info.sizeBytes != byteCount )
		{
			LogMsg( logSeverity_t::Warning, "Baked byte size mismatch: %u != %u", info.sizeBytes, byteCount );
			return false;
		}

		const bool loaded = ( s.Status() == serializeStatus_t::OK );
		assert( loaded );
		return loaded;
	}
	else
	{
		LogMsg( "Baked file not found: %s for asset %s", bakedPath.c_str(), asset.GetName().c_str() );
	}
	return false;
}
