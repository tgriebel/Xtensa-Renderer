#pragma once

#include <unordered_map>
#include <iterator>
#include <string>
#include <functional>
#include <SysCore/common.h>
#include <SysCore/handle.h>
#include <GfxCore/core/util.h>

template< class AssetType >
class Asset;

template< class AssetType >
class AssetLib;

enum loadHandlerFlags_t
{
	LOAD_HANDLER_FLAGS_NONE		= 0,
	LOAD_HANDLER_FLAGS_REBAKE	= ( 1 << 0 ),
};


enum class loadReturn_t : uint8_t
{
	ALREADY_LOADED,
	NO_LOADER,
	LOAD_SUCCESS,
	LOAD_FAIL
};


enum class gpuUploadState_t : uint8_t
{
	NOT_UPLOADED,	// Lives only on CPU or disk
	UPLOAD_QUEUED,	// Needs full GPU upload
	REFRESH_QUEUED,	// Needs lightweight data update. Named "Refresh" to avoid similarity between "Update/Upload" 
	UPLOADED,		// Done.
};


template< class AssetType >
class LoadHandler
{
private:
	uint32_t m_flags = 0;

protected:
	inline void SetFlags( const uint32_t flags )
	{
		m_flags |= flags;
	}

	inline uint32_t GetFlags() const
	{
		return m_flags;
	}

	inline void ClearFlags( const uint32_t flags )
	{
		m_flags &= ~flags;
	}

	inline bool HasFlags( const uint32_t flags ) const
	{
		return ( m_flags & flags ) != 0;
	}

private:
	virtual bool Load( Asset<AssetType>& asset ) = 0;

	friend class Asset<AssetType>;
};

class AssetInterface
{
protected:
	std::string						m_name;
	hdl_t							m_handle;

	gpuUploadState_t				m_uploadState;

	bool							m_loaded;
	bool							m_isDefault;
	bool							m_canBake;

	std::function<void( hdl_t )>	m_uploadCallback;
	std::function<void( hdl_t )>	m_refreshCallback;

	template< class AssetType >
	friend class AssetLib;

	inline void SetUploadCallback( std::function<void( hdl_t )> callback )
	{
		m_uploadCallback = std::move( callback );
	}

	inline void SetRefreshCallback( std::function<void( hdl_t )> callback )
	{
		m_refreshCallback = std::move( callback );
	}

public:
	AssetInterface() : m_loaded( false ), m_isDefault( false ), m_uploadState( gpuUploadState_t::NOT_UPLOADED ), m_canBake( true ), m_handle( INVALID_HDL ) {}

	AssetInterface( const hdl_t hdl ) : m_handle( hdl ), m_loaded( false ), m_isDefault( false ), m_canBake( true ), m_uploadState( gpuUploadState_t::NOT_UPLOADED )
	{}

	AssetInterface( const std::string& _name, const bool _loaded ) :
		m_name( _name ), m_loaded( _loaded ), m_isDefault( false ), m_uploadState( gpuUploadState_t::NOT_UPLOADED ), m_canBake( true )
	{
		m_handle = SysCore::Hash( m_name );
	}

	virtual loadReturn_t Load( const bool rebake = false ) = 0;
	virtual void Unload() = 0;
	virtual loadReturn_t Reload( const bool rebake = false ) = 0;
	virtual bool HasLoader() const = 0;
	virtual void Serialize( Serializer* s ) = 0;

	inline const std::string& GetName() const
	{
		return m_name;
	}

	inline void Rename( const std::string& newName )
	{
		m_name = newName;
		m_handle = Hash( newName );
	}

	inline hdl_t Handle() const
	{
		return m_handle;
	}

	inline bool IsLoaded() const
	{
		return m_loaded;
	}

	inline void SetLoaded()
	{
		m_loaded = true;
	}

	inline void QueueUpload()
	{	
		if( m_uploadCallback )
		{
			m_uploadState = gpuUploadState_t::UPLOAD_QUEUED;
			m_uploadCallback( m_handle );
		}
	}

	inline void RefreshUpload()
	{
		if( m_refreshCallback )
		{
			m_uploadState = gpuUploadState_t::REFRESH_QUEUED;	
			m_refreshCallback( m_handle );
		}
	}

	inline void CompleteUpload()
	{
		m_uploadState = gpuUploadState_t::UPLOADED;
	}

	inline bool IsQueuedForUpload() const
	{
		return ( m_uploadState == gpuUploadState_t::UPLOAD_QUEUED );
	}

	inline bool IsQueuedForRefresh() const
	{
		return ( m_uploadState == gpuUploadState_t::REFRESH_QUEUED );
	}

	inline bool IsUploaded() const
	{
		return ( m_uploadState == gpuUploadState_t::UPLOADED );
	}

	inline bool IsDefault() const
	{
		return m_isDefault;
	}

	inline void SetDefault()
	{
		m_isDefault = true;
	}

	inline bool CanBake() const
	{
		return m_canBake;
	}

	inline void SetBakeable( const bool isBakeable)
	{
		m_canBake = isBakeable;
	}
};


template< class AssetType >
class Asset : public AssetInterface
{
public:
	friend class LoadHandler<AssetType>;
	using loadHandlerPtr_t = std::unique_ptr< LoadHandler<AssetType> >;	

protected:
	loadHandlerPtr_t			m_loader;
	AssetType					m_asset;

	Asset( const hdl_t hdl ) : AssetInterface( hdl ), m_loader( nullptr ) {}

	Asset( const std::string& _name  ) : AssetInterface( _name, false ), m_loader( nullptr ) {}

	Asset( const AssetType& _asset, const std::string& _name, const bool _loaded = true ) :
		AssetInterface( _name, _loaded ), m_asset( _asset ), m_loader( nullptr ) {}

	friend class AssetLib<AssetType>;
public:

	Asset() : AssetInterface(), m_loader( nullptr )
	{}

	inline void AttachLoader( loadHandlerPtr_t _loader )
	{
		m_loader = std::move( _loader );
	}


	inline const AssetType& Get() const
	{
		return m_asset;
	}


	inline AssetType& Get()
	{
		return m_asset;
	}


	bool HasLoader() const override
	{
		return m_loader ? true : false;
	}


	loadReturn_t Load( const bool rebake = false ) override
	{
		if( HasLoader() == false ) {
			return loadReturn_t::NO_LOADER;
		}

		if ( m_loaded == false )
		{
			if( rebake ) {
				m_loader->SetFlags( LOAD_HANDLER_FLAGS_REBAKE );
			}

			m_loaded = m_loader->Load( *this );

			if ( rebake ) {
				m_loader->ClearFlags( LOAD_HANDLER_FLAGS_REBAKE );
			}
			return m_loaded ? loadReturn_t::LOAD_SUCCESS : loadReturn_t::LOAD_FAIL;
		}
		return loadReturn_t::ALREADY_LOADED;
	}


	void Unload() override
	{
	//	m_asset = AssetType{}; // This isn't safe since assets require complex resource management (i.e. deleting API resources)
		m_loaded = false;
	}


	loadReturn_t Reload( const bool rebake = false ) override
	{
		Unload();

		const loadReturn_t ret = Load( rebake );

		if( ret == loadReturn_t::LOAD_SUCCESS ) {
			QueueUpload();
		}

		return ret;
	}


	void Serialize( Serializer* s ) override
	{
		m_asset.Serialize( s );
	}
};
