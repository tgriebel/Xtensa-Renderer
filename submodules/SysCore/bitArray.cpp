#include "bitArray.h"

namespace SysCore
{
void BitArray::Reset()
{
	std::fill( bits.begin(), bits.end(), 0 );
}


void BitArray::Set( const uint32_t element )
{
	const uint32_t arrayElementIx = element / BitsPerElement;

	if ( arrayElementIx >= static_cast<uint32_t>( bits.size() ) )
	{
		bits.resize( arrayElementIx + 1 );
	}

	const uint32_t bitNumber = element % BitsPerElement;
	const ElementType bitMask = ( ElementType( 1 ) << bitNumber );

	bits[ arrayElementIx ] = bits[ arrayElementIx ] | bitMask;
}


void BitArray::Clear( const uint32_t element )
{
	const uint32_t arrayElementIx = element / BitsPerElement;
	const uint32_t bitNumber = element % BitsPerElement;
	const ElementType bitMask = ( ElementType( 1 ) << bitNumber );

	if ( arrayElementIx < static_cast<uint32_t>( bits.size() ) ) {
		bits[ arrayElementIx ] &= ~bitMask;
	}
}


uint32_t BitArray::Count() const
{
	uint32_t count = 0;

	const uint32_t arraySize = static_cast<uint32_t>( bits.size() );
	for ( uint32_t arrayElementIx = 0; arrayElementIx < arraySize; ++arrayElementIx )
	{
		count += SysCore::Popcount( bits[ arrayElementIx ] );
	}
	return count;
}


bool BitArray::AnySet() const
{
	return ( NoneSet() == false );
}


bool BitArray::NoneSet() const
{
	const uint32_t arraySize = static_cast<uint32_t>( bits.size() );
	for ( uint32_t i = 0; i < arraySize; ++i )
	{
		if ( bits[ i ] != 0 ) {
			return false;
		}
	}
	return true;
}


uint32_t BitArray::Size() const
{
	return ( BitsPerElement * static_cast<uint32_t>( bits.size() ) );
}


bool BitArray::IsSet( const uint32_t element ) const
{
	const uint32_t arrayElementIx = element / BitsPerElement;

	if ( arrayElementIx >= bits.size() )
	{
		return false;
	}

	const uint32_t bitNumber = element % BitsPerElement;
	const ElementType bitMask = ( ElementType( 1 ) << bitNumber );

	return ( bits[ arrayElementIx ] & bitMask ) != 0;
}


void TestBitArray()
{
    // --- Construction ---
    {
        BitArray b;
        assert( b.NoneSet() );
        assert( !b.AnySet() );
        assert( b.Count() == 0 );
    }

    {
        BitArray b( 2048 );
        assert( b.Size() == 2048 );
    }

    // --- Set and IsSet ---
    {
        BitArray b;
        b.Set( 0 );
        assert( b.IsSet( 0 ) );
        assert( !b.IsSet( 1 ) );
        assert( b.Count() == 1 );
        assert( b.AnySet() );
        assert( !b.NoneSet() );
    }

    {
        // Last bit of first element
        BitArray b;
        b.Set( 63 );
        assert( b.IsSet( 63 ) );
        assert( !b.IsSet( 62 ) );
        assert( !b.IsSet( 64 ) );
    }

    {
        // First bit of second element
        BitArray b;
        b.Set( 64 );
        assert( b.IsSet( 64 ) );
        assert( !b.IsSet( 63 ) );
        assert( !b.IsSet( 65 ) );
    }

    {
        // Multiple bits
        BitArray b;
        b.Set( 0 );
        b.Set( 1 );
        b.Set( 63 );
        b.Set( 64 );
        assert( b.Count() == 4 );
        assert( b.IsSet( 0 ) );
        assert( b.IsSet( 1 ) );
        assert( b.IsSet( 63 ) );
        assert( b.IsSet( 64 ) );
        assert( !b.IsSet( 2 ) );
    }

    // --- Clear ---
    {
        BitArray b;
        b.Set( 5 );
        assert( b.IsSet( 5 ) );
        b.Clear( 5 );
        assert( !b.IsSet( 5 ) );
        assert( b.Count() == 0 );
    }

    {
        // Clear a bit that was never set
        BitArray b;
        b.Clear( 10 );
        assert( !b.IsSet( 10 ) );
        assert( b.Count() == 0 );
    }

    {
        // Clear only clears the target bit
        BitArray b;
        b.Set( 4 );
        b.Set( 5 );
        b.Set( 6 );
        b.Clear( 5 );
        assert( b.IsSet( 4 ) );
        assert( !b.IsSet( 5 ) );
        assert( b.IsSet( 6 ) );
        assert( b.Count() == 2 );
    }

    // --- Reset ---
    {
        BitArray b;
        b.Set( 0 );
        b.Set( 100 );
        b.Set( 500 );
        b.Reset();
        assert( b.NoneSet() );
        assert( !b.AnySet() );
        assert( b.Count() == 0 );
    }

    // --- Auto resize ---
    {
        BitArray b( 64 );
        b.Set( 1000 );
        assert( b.IsSet( 1000 ) );
        assert( b.Size() >= 1001 );
    }

    // --- IsSet out of bounds ---
    {
        BitArray b( 64 );
        assert( !b.IsSet( 9999 ) );  // Should return false, not crash
    }

    // --- Count ---
    {
        BitArray b;
        for ( uint32_t i = 0; i < 64; ++i ) {
            b.Set( i );
        }
        assert( b.Count() == 64 );
    }

    {
        // Set and clear same bit repeatedly
        BitArray b;
        for ( uint32_t i = 0; i < 100; ++i )
        {
            b.Set( 42 );
            b.Clear( 42 );
        }
        assert( b.Count() == 0 );
        assert( b.NoneSet() );
    }

    // --- Element boundaries ---
    {
        // Every 64th bit (first bit of each element)
        BitArray b;
        for ( uint32_t i = 0; i < 10; ++i ) {
            b.Set( i * 64 );
        }
        assert( b.Count() == 10 );
        for ( uint32_t i = 0; i < 10; ++i ) {
            assert( b.IsSet( i * 64 ) );
        }
    }

    {
        // Every 64th bit - 1 (last bit of each element)
        BitArray b;
        for ( uint32_t i = 0; i < 10; ++i ) {
            b.Set( i * 64 + 63 );
        }
        assert( b.Count() == 10 );
        for ( uint32_t i = 0; i < 10; ++i ) {
            assert( b.IsSet( i * 64 + 63 ) );
        }
    }
}
}