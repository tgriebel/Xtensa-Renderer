#pragma once
#include <string>
#include <algorithm>

#define DBG_SERIALIZER 0

enum class serializeMode_t
{
	STORE,
	LOAD,
};

enum class serializeEndian_t
{
	LITTLE,
	BIG,
};

enum class serializeStatus_t : uint32_t
{
	OK,
	FILE_ERROR,
	FULL_ERROR,
	MODE_ERROR,
	BUFFER_OVERRUN_ERROR,
};

union Convert
{
	Convert() : u64(0) {}
	Convert( const int8_t value )	: i8( value ) {}
	Convert( const uint8_t value )	: u8( value ) {}
	Convert( const bool value )		: b8( value ) {}

	Convert( const int16_t value )	: i16( value ) {}
	Convert( const uint16_t value )	: u16( value ) {}

	Convert( const int32_t value )	: i32( value ) {}
	Convert( const uint32_t value )	: u32( value ) {}
	Convert( const float value )	: f32( value ) {}

	Convert( const int64_t value )	: i64( value ) {}
	Convert( const uint64_t value )	: u64( value ) {}
	Convert( const double value )	: d64( value ) {}

	struct i8_t
	{
		i8_t( const int8_t value ) {
			e[0] = value;
			e[1] = 0;
			e[2] = 0;
			e[3] = 0;
			e[4] = 0;
			e[5] = 0;
			e[6] = 0;
			e[7] = 0;
		}
		int8_t e[ 8 ];
	} i8;

	struct u8_t
	{
		u8_t( const uint8_t value ) {
			e[0] = value;
			e[1] = 0;
			e[2] = 0;
			e[3] = 0;
			e[4] = 0;
			e[5] = 0;
			e[6] = 0;
			e[7] = 0;
		}
		uint8_t e[8];
	} u8;

	struct b8_t
	{
		b8_t( const bool value ) {
			e[0] = value;
			e[1] = 0;
			e[2] = 0;
			e[3] = 0;
			e[4] = 0;
			e[5] = 0;
			e[6] = 0;
			e[7] = 0;
		}
		bool e[8];
	} b8;

	struct i16_t
	{
		i16_t( const int16_t value ) {
			e[0] = value;
			e[1] = 0;
			e[2] = 0;
			e[3] = 0;
		}
		int16_t e[4];
	} i16;

	struct u16_t
	{
		u16_t( const uint16_t value ) {
			e[0] = value;
			e[1] = 0;
			e[2] = 0;
			e[3] = 0;
		}
		uint16_t e[4];
	} u16;

	struct i32_t
	{
		i32_t( const int32_t value ) {
			e[0] = value;
			e[1] = 0;
		}
		int32_t e[2];
	} i32;

	struct u32_t
	{
		u32_t( const uint32_t value ) {
			e[0] = value;
			e[1] = 0;
		}
		uint32_t e[2];
	} u32;

	struct f32_t
	{
		f32_t( const float value ) {
			e[0] = value;
			e[1] = 0;
		}
		float e[2];
	} f32;

	int64_t		i64;
	uint64_t	u64;
	double		d64;
};


struct serializerHeader_t
{
	static const uint32_t MaxSections = 128;
	static const uint32_t MaxNameLength = 128;

	struct section_t
	{
		char		name[ MaxNameLength ];
		uint32_t	offset;
		uint32_t	size;
	};

	section_t	sections[ MaxSections ];
	uint32_t	sectionCount;
};

class Serializer
{
private:
	struct ref_t
	{
		ref_t() = delete;
		ref_t( Convert& _convert, const uint32_t _size ) : convert( _convert ), size( _size ) {  }
		ref_t( const ref_t& ref ) : convert( ref.convert ), size( ref.size ) {}

		Convert& convert;
		uint32_t	size;
	};

	template<typename T>
	ref_t Ref( T& attrib )
	{
		return ref_t( reinterpret_cast<Convert&>( attrib ), sizeof( attrib ) );
	}

	void		Next( Serializer::ref_t type );
	uint32_t	ApplyEndian( const uint32_t index ) const;

public:

	static const uint32_t MaxByteCount = 1073741824;
	static const uint32_t WordLength = 4;

	Serializer( const uint32_t _sizeInBytes, serializeMode_t _mode )
	{
		m_byteCount = std::min( MaxByteCount, _sizeInBytes );
		if( m_byteCount > 0 ) {
			m_bytes = new uint8_t[ _sizeInBytes ];
		} else {
			m_bytes = new uint8_t[ 1 ];
		}
		m_mode = _mode;
		m_endian = serializeEndian_t::LITTLE;
		Clear();
	}

	~Serializer()
	{
		if ( m_bytes != nullptr ) {
			delete[] m_bytes;
		}
		m_byteCount = 0;
		m_mode = serializeMode_t::LOAD;
		SetPosition( 0 );
	}

	Serializer() = delete;
	Serializer( const Serializer& ) = delete;
	Serializer operator=( const Serializer& ) = delete;

	uint8_t*				GetPtr();
	void					SetPosition( const uint32_t index );
	void					Clear( const bool clearMemory = true );
	bool					ReadFile( const std::string& filename );
	bool					WriteFile( const std::string& filename );
	bool					Grow( const uint32_t sizeInBytes );
	uint32_t				CurrentSize() const;
	uint32_t				BufferSize() const;
	bool					CanStore( const uint32_t sizeInBytes ) const;
	void					SetEndian( serializeEndian_t endianMode );
	bool					SetMode( serializeMode_t serializeMode );
	serializeMode_t			GetMode() const;
	serializeStatus_t		Status() const;
	uint64_t				Hash() const;

	uint32_t				NewLabel( const char name[ serializerHeader_t::MaxNameLength ] );
	void					EndLabel( const char name[ serializerHeader_t::MaxNameLength ] );
	bool					FindLabel( const char name[ serializerHeader_t::MaxNameLength ], serializerHeader_t::section_t** outSection );

	inline void				Next( int8_t& value )	{ Next( Ref( value ) ); }
	inline void				Next( uint8_t& value )	{ Next( Ref( value ) ); }
	inline void				Next( bool& value )		{ Next( Ref( value ) ); }
	inline void				Next( int16_t& value )	{ Next( Ref( value ) ); }
	inline void				Next( uint16_t& value )	{ Next( Ref( value ) ); }
	inline void				Next( int32_t& value )	{ Next( Ref( value ) ); }
	inline void				Next( uint32_t& value )	{ Next( Ref( value ) ); }
	inline void				Next( float& value )	{ Next( Ref( value ) ); }
	void					Next( int64_t& value )	{ Next( Ref( value ) ); }
	void					Next( uint64_t& value ) { Next( Ref( value ) ); }
	void					Next( double& value )	{ Next( Ref( value ) ); }
	void					NextArray( uint8_t* u8, uint32_t sizeInBytes );
	void					NextString( std::string& str );

private:
	serializerHeader_t		m_header;
	uint8_t*				m_bytes;
	uint32_t				m_byteCount;
	uint32_t				m_index;
	serializeMode_t			m_mode;
	serializeEndian_t		m_endian;
	serializeStatus_t		m_code;
};

template<class T>
void SerializeStruct( Serializer* s, T& data )
{
	s->NextArray( reinterpret_cast<uint8_t*>( &data ), sizeof( T ) );
}

template<class T>
void SerializeArray( Serializer* s, T data[], const uint32_t elementCount )
{
	s->NextArray( reinterpret_cast<uint8_t*>( data ), sizeof( T ) * elementCount );
}
