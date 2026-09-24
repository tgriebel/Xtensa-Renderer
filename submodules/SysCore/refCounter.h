#pragma once



#pragma once

#include <assert.h>
#include <cstdint>

class refCount_t
{
public:
	refCount_t() = delete;

	refCount_t( const int count ) {
		assert( count > 0 );
		this->count = count;
	}
	inline int Add() {
		return ( count > 0 ) ? ++count : 0;
	}
	inline int Release() {
		return ( count > 0 ) ? --count : 0;
	}
	[[nodiscard]]
	inline int IsFree() const {
		return ( count <= 0 );
	}
private:
	int count; // Considered dead at 0
};