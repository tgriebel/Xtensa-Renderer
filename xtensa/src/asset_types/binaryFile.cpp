#include "binaryFile.h"

#include <SysCore/systemUtils.h>
#include <SysCore/serializer.h>


void BinaryFile::Serialize( Serializer* s )
{
	uint32_t byteCount = static_cast<uint32_t>( data.size() );
	s->Next( byteCount );

	if ( s->GetMode() == serializeMode_t::LOAD ) {
		data.resize( byteCount );
	}

	if ( byteCount > 0 ) {
		s->NextArray( data.data(), byteCount );
	}
}


bool LoadBinaryFileFromDisk( const std::string& path, BinaryFile& outFile )
{
	// Missing is ok to accomodate cache files
	if ( SysCore::FileExists( path ) == false ) {
		outFile = BinaryFile( nullptr, 0, path );
		return true;
	}

	const std::vector<char> bytes = SysCore::ReadBinaryFile( path );
	outFile = BinaryFile( reinterpret_cast<const uint8_t*>( bytes.data() ), bytes.size(), path );
	return true;
}


bool BinaryFileLoader::Load( Asset<BinaryFile>& asset )
{
	return LoadBinaryFileFromDisk( m_path, asset.Get() );
}


bool WriteBinaryFileToDisk( const std::string& path, const BinaryFile& file )
{
	std::string directory, fileName;
	SysCore::SplitPath( path, directory, fileName );

	if ( ( directory.empty() == false ) && ( SysCore::FileExists( directory ) == false ) ) {
		SysCore::MakeDirectory( directory );
	}

	const uint8_t* raw = file.GetRaw();
	const std::vector<char> bytes( raw, raw + file.GetByteCount() );
	return SysCore::WriteBinaryFile( path, bytes );
}
