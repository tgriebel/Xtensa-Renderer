#pragma once
#include <assert.h>
#include <cstdint>

template<class T, uint32_t N>
class Array
{
private:
	T			elements[N];
	T			trap;
	uint32_t	count;
public:

	static const uint32_t Capacity = N;

	Array() : count( 0 )
	{}

	inline T& operator[]( uint32_t index )
	{
		if ( index >= Count() )
		{
			assert( 0 );
			return trap;
		}
		return elements[ index ];
	}


	inline const T& operator[]( uint32_t index ) const
	{
		if ( index >= Count() )
		{
			assert( 0 );
			return trap;
		}
		return elements[ index ];
	}


	inline void Reset()
	{
		count = 0;
	}


	inline T& Resize( uint32_t newCount )
	{
		if( newCount > N ) {
			newCount = N;
		}
		count = newCount;
		return ( count == 0 ) ? elements[ 0 ] : elements[ count - 1 ];
	}


	inline T& Grow( const uint32_t addCount )
	{
		uint32_t newCount = addCount + count;
		if ( newCount > N ) {
			newCount = N;
		}
		count = newCount;
		return ( count == 0 ) ? elements[ 0 ] : elements[ count - 1 ];
	}


	inline T& Shrink( const uint32_t subCount )
	{
		if ( subCount >= count ) {
			count = 0;
		} else {
			count -= subCount;
		}
		return ( count == 0 ) ? elements[ 0 ] : elements[ count - 1 ];
	}


	inline bool Append( const T& element )
	{
		const uint32_t newCount = ( count + 1 );
		if ( newCount <= N )
		{
			elements[ newCount - 1 ] = element;
			count = newCount;
			return true;
		} else {
			assert( 0 );
			return false;
		}
	}


	inline T* Ptr()
	{
		return &elements[ 0 ];
	}


	inline uint32_t Count() const
	{
		return count;
	}
};
