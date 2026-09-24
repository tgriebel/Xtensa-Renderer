#pragma once

#include <cstdint>
#include <string>
#include <cassert>

const uint32_t KB_1 = 1024;
const uint32_t MB_1 = 1024 * KB_1;
const uint32_t GB_1 = 1024 * MB_1;

#define KB( N ) ( N * KB_1 )
#define MB( N ) ( N * MB_1 )
#define GB( N ) ( N * GB_1 )
#define BYTES_TO_KB( N ) ( N / static_cast<float>( KB_1 ) )
#define BYTES_TO_MB( N ) ( N / static_cast<float>( MB_1 ) )
#define BYTES_TO_GB( N ) ( N / static_cast<float>( GB_1 ) )
#define COUNTARRAY( ary ) static_cast<uint32_t>( sizeof( ary ) / sizeof( ary[0] ) )

#define DEFINE_ENUM_OPERATORS( enumType, intType )														\
inline void operator|=( enumType& lhs, const enumType rhs )												\
{																										\
	lhs = static_cast<enumType>( static_cast<intType>( lhs ) | static_cast<intType>( rhs ) );			\
}																										\
																										\
inline void operator&=( enumType& lhs, const enumType rhs )												\
{																										\
	lhs = static_cast<enumType>( static_cast<intType>( lhs ) & static_cast<intType>( rhs ) );			\
}																										\
																										\
inline enumType operator&( const enumType lhs, const enumType rhs )										\
{																										\
	return static_cast<enumType>( static_cast<intType>( lhs ) & static_cast<intType>( rhs ) );			\
}																										\
																										\
inline enumType operator|( const enumType lhs, const enumType rhs )										\
{																										\
	return static_cast<enumType>( static_cast<intType>( lhs ) | static_cast<intType>( rhs ) );			\
}																										\
																										\
inline enumType operator>>( const enumType lhs, const enumType rhs )									\
{																										\
	return static_cast<enumType>( static_cast<intType>( lhs ) >> static_cast<intType>( rhs ) );			\
}																										\
																										\
inline enumType operator<<( const enumType lhs, const enumType rhs )									\
{																										\
	return static_cast<enumType>( static_cast<intType>( lhs ) << static_cast<intType>( rhs ) );			\
}																										\
																										\
inline bool operator==( const enumType lhs, const enumType rhs )										\
{																										\
	return static_cast<intType>( lhs ) == static_cast<intType>( rhs );									\
}																										\
																										\
inline bool operator!=( const enumType lhs, const enumType rhs )										\
{																										\
	return static_cast<intType>( lhs ) != static_cast<intType>( rhs );									\
}																										\
																										\
inline bool operator<( const intType lhs, const enumType rhs )											\
{																										\
	return lhs < static_cast<intType>( rhs );															\
}																										\
																										\
inline bool HasFlags( const enumType& flags, const enumType compareFlags )								\
{																										\
	return ( static_cast<intType>( flags & compareFlags ) != 0 );										\
}																										\
																										\
inline void SetFlags( enumType& flags0, const enumType flags )											\
{																										\
	intType result = static_cast<intType>( flags0 ) | static_cast<intType>( flags );					\
	flags0 = static_cast<enumType>( result );															\
}																										\
																										\
inline void ClearFlags( enumType& flags0, const enumType flags )										\
{																										\
	intType result = static_cast<intType>( flags0 ) & ~static_cast<intType>( flags );					\
	flags0 = static_cast<enumType>( result );															\
}

namespace SysCore
{
// Fowler-Noll-Vo Hash - fnv1a - 32bits
// https://en.wikipedia.org/wiki/Fowler%E2%80%93Noll%E2%80%93Vo_hash_function
static inline uint32_t Hash32( const uint8_t* bytes, const uint32_t sizeBytes )
{
	uint32_t result = 2166136261;
	const uint32_t prime = 16777619;
	for ( uint32_t i = 0; i < sizeBytes; ++i ) {
		result = ( result ^ bytes[ i ] ) * prime;
	}
	return result;
}


// Fowler-Noll-Vo Hash - fnv1a - 64bits
static inline uint64_t Hash( const uint8_t* bytes, const uint64_t sizeBytes )
{
	uint64_t result = 14695981039346656037ULL;
	const uint64_t prime = 1099511628211ULL;
	for( uint64_t i = 0; i < sizeBytes; ++i ) {
		result = ( result ^ bytes[ i ] ) * prime;
	}
	return result;
}


// Polynomial Rolling hash
static inline uint64_t Hash( const std::string& s )
{
	const int p = 31;
	const int m = static_cast<int>( 1e9 + 9 );
	uint64_t hash = 0;
	uint64_t pN = 1;
	const int stringLen = static_cast<int>( s.size() );
	for ( int i = 0; i < stringLen; ++i )
	{
		hash = ( hash + ( s[ i ] - (uint64_t)'a' + 1ull ) * pN ) % m;
		pN = ( pN * p ) % m;
	}
	return hash;
}


static inline uint32_t Align( const uint64_t size, const uint64_t alignment )
{
	const uint32_t alignedSize = static_cast<uint32_t>( ( size + ( alignment - 1 ) ) & ~( alignment - 1 ) );
	assert( ( alignedSize % alignment ) == 0 );
	return alignedSize;
}


static inline uint32_t RoundUpToPowerOfTwo( uint32_t value )
{
	if ( value == 0 ) return 1;
	--value;
	value |= value >> 1;
	value |= value >> 2;
	value |= value >> 4;
	value |= value >> 8;
	value |= value >> 16;
	return ++value;
}


static inline uint32_t Popcount( const uint64_t value )
{
	uint64_t x = value;

	x = x - ( ( x >> 1 ) & 0x5555555555555555ull );
	x = ( x & 0x3333333333333333ull ) + ( ( x >> 2 ) & 0x3333333333333333ull );
	x = ( x + ( x >> 4 ) ) & 0x0F0F0F0F0F0F0F0Full;
	return ( x * 0x0101010101010101ull ) >> 56;
}
};
