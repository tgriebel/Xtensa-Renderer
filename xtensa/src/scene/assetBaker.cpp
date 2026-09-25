#include "assetBaker.h"

#include <chrono>
#include <ctime>
#include <fstream>
#include <filesystem>
#include <chrono>
#include <syscore/common.h>

#include <SysCore/serializer.h>
#include <SysCore/systemUtils.h>

#include "../asset_types/model.h"
#include "../asset_types/material.h"
#include "../asset_types/image.h"
#include "../asset_types/gpuProgram.h"
#include <gfxcore/io/serializeClasses.h>

#include "../asset_types/assetLib.h"
#include "../globals/common.h"
#include "../globals/assetDefs.h"

using namespace SysCore;

static std::vector<bakedAssetInfo_t> assetInfo;


static int64_t ToEpochSeconds( const std::chrono::system_clock::time_point& tp )
{
	return std::chrono::duration_cast<std::chrono::seconds>( tp.time_since_epoch() ).count();
}


static bool GetFileDateInSeconds( const std::string& path, int64_t& epochSeconds )
{
	std::error_code ec;
	const auto srcFileTime = std::filesystem::last_write_time( path, ec );
	if( ec ) {
		return false;
	}

	const auto clockDelta = srcFileTime - std::filesystem::file_time_type::clock::now();
	const auto sysTime    = std::chrono::system_clock::now()
		+ std::chrono::duration_cast<std::chrono::system_clock::duration>( clockDelta );

	epochSeconds = ToEpochSeconds( sysTime );
	return true;
}


static bool ExtractSecondsFromTimestamp( const std::string& date, int64_t& epochSeconds )
{
	epochSeconds = 0;
	try {
		epochSeconds = std::stoll( date );
	}
	catch( ... ) {
		return false;
	}
	return true;
}


static std::string BuildCurrentSystemTimestamp()
{
	const auto now       = std::chrono::system_clock::now();
	const std::time_t t  = std::chrono::system_clock::to_time_t( now );

	char buf[ 64 ];
	ctime_s( buf, sizeof( buf ), &t );
	std::string dateStr( buf );
	if ( !dateStr.empty() && dateStr.back() == '\n' ) { dateStr.pop_back(); }

	return std::to_string( ToEpochSeconds( now ) ) + " (" + dateStr + ")";
}


bool IsBakedAssetFresh( const sourceFile_t& source, const bakedAssetInfo_t& bakedInfo )
{
	// No source file (e.g. code-generated asset)
	if( source.isBakedAsset ) {
		return true;
	}

	// Check if the name mismatches
	// If there is no source path, the baked asset is referenced and loaded directly
	const std::string bakedStem = std::filesystem::path( bakedInfo.name ).stem().string();
	const std::string sourceStem = std::filesystem::path( source.name ).stem().string();

	if( ( source.name.empty() == false ) && ( bakedStem != sourceStem ) ) {
		return false;
	}

	if( source.path.empty() ) {
		return true;
	}

	int64_t bakeEpoch = 0;
	ExtractSecondsFromTimestamp( bakedInfo.date, bakeEpoch );

	// FIXME: Cubemaps use a special convention for source files and can have 6 sources. Need a solution for this
	// Maybe store list of source paths in baked asset then check each one?
	int64_t srcEpoch;
	if( GetFileDateInSeconds( source.path, srcEpoch ) == false ) {
		return false;
	}

	return ( srcEpoch <= bakeEpoch );
}


template<class T>
static void BakeLibraryAssets( AssetLib<T>& lib, const std::string& path, const std::string& ext )
{
	const std::string bakeDate = BuildCurrentSystemTimestamp();

	const uint32_t count = lib.Count();
	for ( uint32_t i = 0; i < count; ++i )
	{
		Asset<T>& asset = *lib.Find( i );

		bakedAssetInfo_t info = {};
		info.date = bakeDate;
		info.type = lib.AssetTypeName();

		if( StoreBaked( asset, info, path, ext ) == false ) {
			continue;
		}

		assetInfo.push_back( info );
	}
}


void AssetBaker::AddAssetLib( AssetLib<Model>* lib, const std::string path, const std::string ext )
{
	m_modelLib = lib;
	m_modelPath = path;
	m_modelExt = ext;
}


void AssetBaker::AddAssetLib( AssetLib<Material>* lib, const std::string path, const std::string ext )
{
	m_materialLib = lib;
	m_materialPath = path;
	m_materialExt = ext;
}


void AssetBaker::AddAssetLib( AssetLib<Image>* lib, const std::string path, const std::string ext )
{
	m_imageLib = lib;
	m_imagePath = path;
	m_imageExt = ext;
}


void AssetBaker::AddBakeDirectory( const std::string path )
{
	m_bakePath = path;
}


void AssetBaker::Bake()
{
	assetInfo.reserve(	( m_imageLib ? m_imageLib->Count() : 0 ) +
						( m_materialLib ? m_materialLib->Count() : 0 ) +
						( m_modelLib ? m_modelLib->Count() : 0 ) );

	MakeDirectory( m_bakePath );	

	if( m_imageLib != nullptr )
	{
		MakeDirectory( m_bakePath + m_imagePath );
		BakeLibraryAssets( *m_imageLib, m_bakePath + m_imagePath, m_imageExt );
	}

	if ( m_materialLib != nullptr )
	{
		MakeDirectory( m_bakePath + m_materialPath );
		BakeLibraryAssets( *m_materialLib, m_bakePath + m_materialPath, m_materialExt );
	}

	if ( m_modelLib != nullptr )
	{
		MakeDirectory( m_bakePath + m_modelPath );
		BakeLibraryAssets( *m_modelLib, m_bakePath + m_modelPath, m_modelExt );
	}

	std::ofstream assetFile( m_bakePath + "asset_info.csv", std::ios::out | std::ios::trunc );
	assetFile << "Name,Type,Asset Hash,Data Hash,Size,Date\n";
	for ( auto it = assetInfo.begin(); it != assetInfo.end(); ++it )
	{
		const bakedAssetInfo_t& asset = *it;
		assetFile << asset.name << "," << asset.type << "," << asset.hash << "," << std::to_string( asset.dataHash ) << "," << asset.sizeBytes << "," << asset.date << "\n";
	}
	assetFile.close();
}


void BakeAssets()
{
	AssetBaker baker;
	baker.AddBakeDirectory( BakePath );
	baker.AddAssetLib( &ModelLib(), ModelPath, BakedModelExtension );
	baker.AddAssetLib( &MaterialLib(), MaterialPath, BakedMaterialExtension );
	baker.AddAssetLib( &ImageLib(), TexturePath, BakedTextureExtension );

	baker.Bake();
}
