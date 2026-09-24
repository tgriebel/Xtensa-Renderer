#pragma once



#include <assert.h>
#include <cstdint>
#include <vector>

#include "common.h"

namespace SysCore
{
void TestBitArray();

class BitArray
{
private:

	using ElementType = uint64_t;
	static constexpr uint32_t BitsPerElement = 8 * sizeof( ElementType );

	std::vector<ElementType> bits;

public:

	BitArray( uint32_t reserveBits = 1024 )
	{
		bits.resize( ( reserveBits + BitsPerElement - 1 ) / BitsPerElement );
	}

	void			Reset();

	void			Set( const uint32_t element );

	void			Clear( const uint32_t element );

	[[nodiscard]]
	uint32_t		Count() const;

	[[nodiscard]]
	bool			AnySet() const;

	[[nodiscard]]
	bool			NoneSet() const;

	[[nodiscard]]
	uint32_t		Size() const;

	[[nodiscard]]
	bool			IsSet( const uint32_t element ) const;
};
}