#include "binaryFile.h"

#include <fstream>
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


bool BinaryFileLoader::Load( Asset<BinaryFile>& asset )
{
	BinaryFile& file = asset.Get();
	file.data.clear();

	if ( SysCore::FileExists( m_path ) == false ) {
		return true;
	}

	std::ifstream stream( m_path, std::ios::binary | std::ios::ate );
	const std::streamsize size = stream.tellg();
	if ( size > 0 )
	{
		file.data.resize( static_cast<size_t>( size ) );
		stream.seekg( 0 );
		stream.read( reinterpret_cast<char*>( file.data.data() ), size );
	}
	return true;
}
