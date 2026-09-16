#pragma once
#include <algorithm>
#include <list>
#include <unordered_map>
#include <iterator>
#include <sstream>
#include <thread>
#include <mutex>
#include <vector>

#include "asset.h"

#include <SysCore/handle.h>
#include <GfxCore/core/util.h>
#include "../app/cvar.h"
#include "../globals/common.h"
#include <SysCore/common.h>

extern CVar s_threadedLoad;

struct AssetListEntry
{
	std::string	name;
	hdl_t		handle;
};

class Library
{
public:
	static inline hdl_t					Handle( const char* name ) { return SysCore::Hash( name ); }
	virtual const char*					AssetTypeName() const = 0;
	virtual void						Clear() = 0;
	virtual bool						SetDefault( const hdl_t& hdl ) = 0;
	virtual bool						SetDefault( const char* name ) = 0;
	virtual AssetInterface*				GetDefault() = 0;
	virtual const AssetInterface*		GetDefault() const = 0;
	virtual void						LoadAll( const bool rebake = false ) = 0;
	virtual void						UnloadAll() = 0;
	virtual bool						HasPendingLoads() const = 0;
	virtual uint32_t					Count() const = 0;
	virtual bool						Exists( const char* name ) const = 0;
	virtual bool						Exists( const hdl_t& hdl ) const = 0;
	virtual AssetInterface*				Find( const char* name ) = 0;
	virtual const AssetInterface*		Find( const char* name ) const = 0;
	virtual AssetInterface*				Find( const uint32_t id ) = 0;
	virtual const AssetInterface*		Find( const uint32_t id ) const = 0;
	virtual AssetInterface*				Find( const hdl_t& hdl ) = 0;
	virtual const AssetInterface*		Find( const hdl_t& hdl ) const = 0;
	virtual const char*					FindName( const hdl_t& hdl ) const = 0;
	virtual const char*					FindName( const uint32_t id ) const = 0;
	virtual hdl_t						RetrieveHdl( const char* name ) const = 0;
};

static std::mutex mtx;

template< class AssetType >
class AssetLib : public Library
{
private:
	using loadList_t = std::list<uint64_t>;
	using assetMap_t = std::unordered_map< uint64_t, Asset<AssetType> >;

	std::string	typeName;
	loadList_t	pendingLoad;
	assetMap_t	assets;
	hdl_t		defaultHdl;
public:
	AssetLib()
	{}

	AssetLib( const char* assetTypeName )
	{
		typeName = assetTypeName;
		defaultHdl = INVALID_HDL;
		assets.clear();
		pendingLoad.clear();
	}

	const char* AssetTypeName() const
	{
		return typeName.c_str();
	}

	static inline hdl_t			Handle( const char* name ) { return ( name == nullptr ) ? INVALID_HDL : SysCore::Hash( name ); }
	void						Clear();
	bool						SetDefault( const hdl_t& hdl );
	bool						SetDefault( const char* name );
	Asset<AssetType>*			GetDefault() { return ( defaultHdl != INVALID_HDL ) ? Find( defaultHdl ) : nullptr; };
	const Asset<AssetType>*		GetDefault() const { return ( defaultHdl != INVALID_HDL ) ? Find( defaultHdl ) : nullptr; };
	void						LoadAll( const bool rebake = false );
	void						UnloadAll();
	bool						HasPendingLoads() const { return ( pendingLoad.size() > 0 ); }
	uint32_t					Count() const { return static_cast<uint32_t>( assets.size() ); }
	hdl_t						Add( const char* name, const AssetType& asset, const bool replaceIfFound = false );
	hdl_t						AddDeferred( const char* name, std::unique_ptr< LoadHandler<AssetType> > loader = std::unique_ptr< LoadHandler<AssetType> >() );
	bool						AddDeferred( const hdl_t hdl, std::unique_ptr< LoadHandler<AssetType> > loader = std::unique_ptr< LoadHandler<AssetType> >() );
	void						Remove( const uint32_t id );
	void						Remove( const hdl_t& hdl );
	bool						Exists( const char* name ) const;
	bool						Exists( const hdl_t& hdl ) const;
	Asset<AssetType>*			Find( const char* name );
	const Asset<AssetType>*		Find( const char* name ) const;
	Asset<AssetType>*			Find( const uint32_t id );
	const Asset<AssetType>*		Find( const uint32_t id ) const;
	Asset<AssetType>*			Find( const hdl_t& hdl );
	const Asset<AssetType>*		Find( const hdl_t& hdl ) const;
	const char*					FindName( const hdl_t& hdl ) const;
	const char*					FindName( const uint32_t id ) const;
	hdl_t						RetrieveHdl( const char* name ) const;
	void						GetAssetList( std::vector<AssetListEntry>& outList ) const;
};


template< class AssetType >
void AssetLib< AssetType >::Clear()
{
	UnloadAll();
	assets.clear();
	pendingLoad.clear();
}


static inline void ThreadLoad( AssetInterface* asset, const bool rebake = false )
{
	asset->Load( rebake );
}

template< class AssetType >
void AssetLib< AssetType >::LoadAll( const bool rebake )
{
	const bool useThreading = s_threadedLoad.GetBool();

	std::vector< std::thread > threadPool;
	threadPool.reserve( pendingLoad.size() );

	for ( hdl_t handle : pendingLoad )
	{
		Asset<AssetType>* asset = Find( handle );
		
		if( useThreading ) {
			threadPool.push_back( std::thread( ThreadLoad, asset, rebake ) );
		} else {
			ThreadLoad( asset );
		}
	}

	for ( auto& thread : threadPool )
	{
		thread.join();
	}

	for ( hdl_t handle : pendingLoad )
	{
		Asset<AssetType>* asset = Find( handle );
		if( asset->IsLoaded() == false ) {
			Remove( handle ); // Assume this is a bad asset, path, or loader
		} else {		
			asset->QueueUpload();
		}
	}
	pendingLoad.clear();
}

template< class AssetType >
void AssetLib< AssetType >::UnloadAll()
{
	for ( auto it = assets.begin(); it != assets.end(); ++it )
	{
		Asset<AssetType>& asset = it->second;
		if( asset.HasLoader() )
		{
			asset.Unload();
			pendingLoad.push_back( it->first );
		}
	}
}

template< class AssetType >
hdl_t AssetLib< AssetType >::Add( const char* name, const AssetType& asset, const bool replaceIfFound )
{
	std::string assetName = name;
	if( assetName.length() == 0 ) {
		THROW_ERROR( "AssetLib::Add() - asset name is empty!" );
	}
	
	uint64_t hash = SysCore::Hash( assetName.c_str() );
	
	std::unique_lock<std::mutex> lock( mtx, std::defer_lock );
	lock.lock();

	if( replaceIfFound == false )
	{
		uint64_t instance = 0;
		auto it = assets.find( hash );
		while( it != assets.end() )
		{
			++instance;
			std::stringstream ss;
			ss << name << "_" << instance;
			assetName = ss.str();
		
			hash = SysCore::Hash( assetName.c_str() );
			it = assets.find( hash );
		}
	}
	assets[ hash ] = Asset<AssetType>( asset, assetName );

	lock.unlock();

	return Handle( name );
}

template< class AssetType >
hdl_t AssetLib< AssetType >::AddDeferred( const char* name, std::unique_ptr< LoadHandler<AssetType> > loader )
{
	std::string assetName = name;
	if ( assetName.length() == 0 ) {
		THROW_ERROR( "AssetLib::AddDeferred() - asset name is empty!" );
	}

	std::unique_lock<std::mutex> lock( mtx, std::defer_lock );
	lock.lock();

	const uint64_t hash = SysCore::Hash( name );
	auto it = assets.find( hash );
	if ( it == assets.end() ) {
		assets[ hash ] = Asset<AssetType>( assetName );
		pendingLoad.push_back( hash );
	}

	lock.unlock();

	Asset<AssetType>& asset = assets[ hash ];
	if( loader ) {
		asset.AttachLoader( std::move( loader ) );
	}

	return Handle( name );
}

template< class AssetType >
bool AssetLib<AssetType>::AddDeferred( const hdl_t hdl, std::unique_ptr< LoadHandler<AssetType> > loader )
{
	if ( hdl == INVALID_HDL ) {
		return false;
	}

	std::unique_lock<std::mutex> lock( mtx, std::defer_lock );
	lock.lock();

	const uint64_t hash = hdl.Get();
	auto it = assets.find( hash );
	if ( it == assets.end() ) {
		assets[ hash ] = Asset<AssetType>( hdl );
		pendingLoad.push_back( hash );
	}

	lock.unlock();

	Asset<AssetType>& asset = assets[ hash ];
	if ( loader ) {
		asset.AttachLoader( std::move( loader ) );
	}

	return true;
}

template< class AssetType >
void AssetLib<AssetType>::Remove( const uint32_t id )
{
	std::unique_lock<std::mutex> lock( mtx, std::defer_lock );
	lock.lock();

	auto it = assets.begin();
	std::advance( it, id );
	assets.erase( it );

	lock.unlock();
}

template< class AssetType >
void AssetLib<AssetType>::Remove( const hdl_t& hdl )
{
	std::unique_lock<std::mutex> lock( mtx, std::defer_lock );
	lock.lock();

	assets.erase( hdl.Get() );

	lock.unlock();
}

template< class AssetType >
bool AssetLib< AssetType >::SetDefault( const hdl_t& hdl )
{
	if( Exists( hdl ) )
	{
		defaultHdl = hdl;
		return true;
	}
	return false;
}

template< class AssetType >
bool AssetLib< AssetType >::SetDefault( const char* name )
{
	return SetDefault( Handle( name ) );
}

template< class AssetType >
bool AssetLib< AssetType >::Exists( const hdl_t& hdl ) const
{
	auto it = assets.find( hdl.Get() );
	return ( it != assets.end() );
}

template< class AssetType >
bool AssetLib< AssetType >::Exists( const char* name ) const
{
	auto it = assets.find( SysCore::Hash( name ) );
	return ( it != assets.end() );
}

template< class AssetType >
Asset<AssetType>* AssetLib< AssetType >::Find( const char* name )
{
	auto it = assets.find( SysCore::Hash( name ) );
	return ( it != assets.end() ) ? &it->second : GetDefault();
}

template< class AssetType >
const Asset<AssetType>* AssetLib< AssetType >::Find( const char* name ) const
{
	auto it = assets.find( SysCore::Hash( name ) );
	return ( it != assets.end() ) ? &it->second : GetDefault();
}

template< class AssetType >
Asset<AssetType>* AssetLib< AssetType >::Find( const uint32_t id )
{
	auto it = assets.begin();
	std::advance( it, id );
	return ( it != assets.end() ) ? &it->second : GetDefault();
}

template< class AssetType >
const Asset<AssetType>* AssetLib< AssetType >::Find( const uint32_t id ) const
{
	auto it = assets.begin();
	std::advance( it, id );
	return ( it != assets.end() ) ? &it->second : GetDefault();
}

template< class AssetType >
Asset<AssetType>* AssetLib< AssetType >::Find( const hdl_t& hdl )
{
	auto it = assets.find( hdl.Get() );
	return ( it != assets.end() ) ? &it->second : GetDefault();
}

template< class AssetType >
const Asset<AssetType>* AssetLib< AssetType >::Find( const hdl_t& hdl ) const
{
	auto it = assets.find( hdl.Get() );
	return ( it != assets.end() ) ? &it->second : GetDefault();
}

template< class AssetType >
const char* AssetLib< AssetType >::FindName( const hdl_t& hdl ) const
{
	auto it = assets.find( hdl.Get() );
	return ( it != assets.end() ) ? it->second.GetName().c_str() : "<missing-asset>";
}

template< class AssetType >
const char* AssetLib< AssetType >::FindName( const uint32_t id ) const
{
	auto it = assets.begin();
	std::advance( it, id );
	return ( it != assets.end() ) ? it->second.GetName().c_str() : "<missing-asset>";
}

template< class AssetType >
hdl_t AssetLib< AssetType >::RetrieveHdl( const char* name ) const
{
	const uint64_t hash = SysCore::Hash( name );
	auto it = assets.find( hash );
	return ( it != assets.end() ) ? hdl_t( hash ) : INVALID_HDL;
}

template< class AssetType >
void AssetLib< AssetType >::GetAssetList( std::vector<AssetListEntry>& outList ) const
{
	outList.clear();
	outList.reserve( assets.size() );

	for ( const auto& kv : assets )
	{
		AssetListEntry entry;
		entry.name   = kv.second.GetName();
		entry.handle = hdl_t( kv.first );
		outList.push_back( entry );
	}

	std::sort( outList.begin(), outList.end(),
		[]( const AssetListEntry& a, const AssetListEntry& b ) {
			return a.name < b.name;
		} );
}
