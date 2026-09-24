#pragma once

#include <assert.h>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <string>
#include <type_traits>

template<typename T>
class Handle
{
public:
	static const T InvalidValue = ~0ull;

	Handle() = default;

	Handle( const T handle )
	{
		static_assert( std::is_trivially_copyable<Handle<T>>::value, "Handle<T> must be trivially copyable" );
		this->value = handle;
	}

	bool operator==( const Handle& rhs ) const {
		return ( value == rhs.value );
	}

	bool operator!=( const Handle& rhs ) const {
		return ( value != rhs.value );
	}

	bool operator<( const Handle& rhs )  const {
		return ( value < rhs.value );
	}

	bool operator<=( const Handle& rhs ) const {
		return ( value <= rhs.value );
	}

	bool operator>( const Handle& rhs )  const {
		return ( value > rhs.value );
	}

	bool operator>=( const Handle& rhs ) const {
		return ( value >= rhs.value );
	}

	std::string String() const
	{
		std::stringstream ss;
		ss << std::setw( 2 * sizeof( T ) ) << std::setfill( '0' ) << Get();
		return ss.str();
	}

	void Reset() {
		value = InvalidValue;
	}

	bool IsValid() const {
		return ( value != InvalidValue );
	}

	T Get() const {
		return IsValid() ? value : InvalidValue;
	}
private:
	T	value;
};

using hdl8_t  = Handle<uint8_t>;
using hdl16_t = Handle<uint16_t>;
using hdl32_t = Handle<uint32_t>;
using hdl64_t = Handle<uint64_t>;
using hdl_t   = hdl64_t;

#define INVALID_HDL hdl_t( hdl_t::InvalidValue )
