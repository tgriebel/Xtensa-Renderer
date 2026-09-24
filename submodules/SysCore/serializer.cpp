#include <iostream>
#include <fstream>
#include <vector>
#include <stdio.h>
#include <sstream>
#include "serializer.h"
#include "assert.h"
#include "common.h"

uint8_t* Serializer::GetPtr()
{
	return m_bytes;
}


void Serializer::SetPosition( const uint32_t index )
{
	m_index = index;
}


void Serializer::Clear( const bool clearMemory )
{
	if( clearMemory && ( m_bytes != nullptr ) ) {
		memset( m_bytes, 0, BufferSize() );
	}
	memset( &m_header, 0, sizeof( serializerHeader_t ) );
	SetPosition( 0 );
	m_code = serializeStatus_t::OK;
}


bool Serializer::ReadFile( const std::string& filename )
{
	std::ifstream file( filename, std::ios::in | std::ios::ate | std::ios::binary );

	if ( !file.is_open() ) {
		return false;
	}

	uint32_t fileSize = static_cast<uint32_t>( file.tellg() );
	if( CanStore( fileSize ) == false ) {
		if( Grow( fileSize - m_byteCount ) == false ) {
			return false;
		}
	}

	file.seekg( 0 );
	file.read( reinterpret_cast<char*>( m_bytes ), fileSize );
	file.close();

	return true;
}


bool Serializer::WriteFile( const std::string& filename )
{
	std::ofstream file( filename, std::ios::out | std::ios::trunc | std::ios::binary );

	if ( !file.is_open() )
	{
		m_code = serializeStatus_t::FILE_ERROR;
		return false;
	}
	file.write( reinterpret_cast<char*>( m_bytes ), CurrentSize() );
	file.close();

	return true;
}


bool Serializer::Grow( const uint32_t sizeInBytes )
{
	if( sizeInBytes == 0 ) {
		return true;
	}

	const uint64_t chkSize = m_byteCount + static_cast<uint64_t>( sizeInBytes );
	if( chkSize >= static_cast<uint64_t>( MaxByteCount ) )
	{
		m_code = serializeStatus_t::FULL_ERROR;
		return false;
	}

	const uint32_t oldCount = m_byteCount;
	m_byteCount += sizeInBytes;

	uint8_t* newBytes = new uint8_t[ m_byteCount ];
	if( m_bytes != nullptr ) {
		memcpy( newBytes, m_bytes, oldCount );
		delete[] m_bytes;
	}
	memset( newBytes + oldCount, 0, sizeInBytes );

	m_bytes = newBytes;
	return true;
}


uint32_t Serializer::CurrentSize() const
{
	return m_index;
}


uint32_t Serializer::BufferSize() const
{
	return m_byteCount;
}


bool Serializer::CanStore( const uint32_t sizeInBytes ) const
{
	return ( CurrentSize() + sizeInBytes <= BufferSize() );
}


void Serializer::SetEndian( serializeEndian_t endianMode )
{
	m_endian = endianMode;
}


bool Serializer::SetMode( serializeMode_t serializeMode )
{
	if( m_index > 0 )
	{
		m_code = serializeStatus_t::MODE_ERROR;
		return false;
	}
	m_mode = serializeMode;
	return true;
}


serializeMode_t Serializer::GetMode() const
{
	return m_mode;
}


serializeStatus_t Serializer::Status() const
{
	return m_code;
}


uint64_t Serializer::Hash() const
{
	return SysCore::Hash( m_bytes, CurrentSize() );
}


uint32_t Serializer::ApplyEndian( const uint32_t index ) const
{
	if ( m_endian == serializeEndian_t::BIG )
	{
		const uint32_t wordBase = ( m_index / WordLength ) * WordLength;
		const uint32_t byteOffset = ( WordLength - 1 ) - ( m_index % WordLength );
		return ( wordBase + byteOffset );
	}
	else
	{
		return index;
	}
}


uint32_t Serializer::NewLabel( const char name[ serializerHeader_t::MaxNameLength ] )
{
	const uint32_t sectionIx = m_header.sectionCount;

	serializerHeader_t::section_t& section = m_header.sections[ sectionIx ];
	section.offset = m_index;
	strcpy_s< serializerHeader_t::MaxNameLength >( section.name, name );
	++m_header.sectionCount;

	return sectionIx;
}


void Serializer::EndLabel( const char name[ serializerHeader_t::MaxNameLength ] )
{
	serializerHeader_t::section_t* section;
	if ( FindLabel( name, &section ) )
	{
		section->size = ( m_index - section->offset );
	}
}


bool Serializer::FindLabel( const char name[ serializerHeader_t::MaxNameLength ], serializerHeader_t::section_t** outSection )
{
	for ( uint32_t i = 0; i < m_header.sectionCount; ++i )
	{
		serializerHeader_t::section_t& section = m_header.sections[ i ];
		if ( _strnicmp( name, section.name, serializerHeader_t::MaxNameLength ) == 0 )
		{
			*outSection = &section;
			return true;
		}
	}

	*outSection = nullptr;
	return false;
}


void Serializer::Next( Serializer::ref_t type )
{
	if ( CanStore( type.size ) == false )
	{
		m_code = serializeStatus_t::BUFFER_OVERRUN_ERROR;
		return;
	}

	if ( m_mode == serializeMode_t::LOAD )
	{
		for ( uint32_t i = 0; i < type.size; ++i )
		{
			type.convert.u8.e[ i ] = m_bytes[ ApplyEndian( m_index ) ];
			++m_index;
		}
	}
	else if( m_mode == serializeMode_t::STORE )
	{
		for ( uint32_t i = 0; i < type.size; ++i )
		{
			m_bytes[ ApplyEndian( m_index ) ] = type.convert.u8.e[ i ];
			++m_index;
		}
	}
}


void Serializer::NextArray( uint8_t* u8, uint32_t sizeInBytes )
{
	if ( CanStore( sizeInBytes ) == false )
	{
		m_code = serializeStatus_t::BUFFER_OVERRUN_ERROR;
		return;
	}

	if ( m_mode == serializeMode_t::LOAD ) {
		if ( m_endian == serializeEndian_t::BIG )
		{
			for ( uint32_t i = 0; i < sizeInBytes; ++i )
			{
				u8[ i ] = m_bytes[ ApplyEndian( m_index ) ];
				++m_index;
			}
		}
		else
		{
			memcpy( u8, m_bytes + m_index, sizeInBytes );
			m_index += sizeInBytes;
		}
	}
	else if ( m_mode == serializeMode_t::STORE )
	{
		if ( m_endian == serializeEndian_t::BIG )
		{
			for ( uint32_t i = 0; i < sizeInBytes; ++i )
			{
				m_bytes[ ApplyEndian( m_index ) ] = u8[ i ];
				++m_index;
			}
		}
		else
		{
			memcpy( m_bytes + m_index, u8, sizeInBytes );
			m_index += sizeInBytes;
		}
	}
}


void Serializer::NextString( std::string& str )
{
	
	if ( m_mode == serializeMode_t::LOAD )
	{
		uint32_t length;
		Next( length );
		char* buffer = new char[ length + 1 ];
	
		NextArray( reinterpret_cast<uint8_t*>( buffer ), length );
		buffer[ length ] = '\0';
		str = buffer;
		delete[] buffer;
	}
	else
	{
		uint32_t length = static_cast<uint32_t>( str.length() );
		Next( length );
		char* buffer = new char[ length + 1 ];
		str.copy( buffer, length );
		buffer[ length ] = '\0';

		NextArray( reinterpret_cast<uint8_t*>( buffer ), length );
		delete[] buffer;
	}

}
