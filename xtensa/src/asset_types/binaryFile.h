#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "asset.h"

class Serializer;


// Generic binary file. Enforces a standard path for file loading and serialization
class BinaryFile
{
private:
	std::vector<uint8_t>	data;
	std::string				path;

public:
	BinaryFile() {}

	BinaryFile( const uint8_t* bytes, const size_t byteCount, const std::string& sourcePath = "" ) : path( sourcePath )
	{
		if ( byteCount > 0 ) {
			data.assign( bytes, bytes + byteCount );
		}
	}

	uint32_t			GetByteCount() const { return static_cast<uint32_t>( data.size() ); }
	const uint8_t*		GetRaw() const { return data.data(); }
	const std::string&	GetPath() const { return path; }

	void Serialize( Serializer* s );
};


bool LoadBinaryFileFromDisk( const std::string& path, BinaryFile& outFile );
bool WriteBinaryFileToDisk( const std::string& path, const BinaryFile& file );


class BinaryFileLoader : public LoadHandler<BinaryFile>
{
private:
	std::string		m_path;

	bool Load( Asset<BinaryFile>& asset ) override;

public:
	BinaryFileLoader() {}
	BinaryFileLoader( const std::string& path ) : m_path( path ) {}

	void SetPath( const std::string& path ) { m_path = path; }
};

using pBinaryFileLoader_t = Asset<BinaryFile>::loadHandlerPtr_t;
