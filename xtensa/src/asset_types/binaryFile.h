#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "asset.h"

class Serializer;


// Generic binary file. Enforces a standard path for file loading and serialization
class BinaryFile
{
public:
	std::vector<uint8_t>	data;

	void Serialize( Serializer* s );
};


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
