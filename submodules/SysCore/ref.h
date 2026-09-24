#pragma once

#include "refCounter.h"
#include <assert.h>
#include <cstdint>

template<typename T>
class ref_t
{
public:
	ref_t()
	{
		this->object = nullptr;
		this->instances = nullptr;
	};

	ref_t( const T* obj )
	{
		this->object = obj;
		this->instances = new refCount_t( 1 );
	}

	ref_t( const ref_t& handle )
	{
		if ( handle.IsValid() )
		{
			this->object = handle.object;
			this->instances = handle.instances;
			this->instances->Add();
		}
		else {
			this->object = nullptr;
			this->instances = nullptr;
		}
	}

	~ref_t()
	{
		if ( IsValid() )
		{
			instances->Release();
			if ( instances->IsFree() )
			{
				delete instances;
			}
			instances = nullptr;
		}
	}

	ref_t& operator=( const ref_t& handle )
	{
		if ( this != &handle )
		{
			this->~ptr_t();
			this->object = handle.object;
			this->instances = handle.instances;
			if ( handle.IsValid() ) {
				this->instances->Add();
			}
		}
		return *this;
	}

	bool operator==( const ref_t& rhs ) const {
		return ( object == rhs.object );
	}

	bool operator!=( const ref_t& rhs ) const {
		return ( object != rhs.object );
	}

	const T* operator->() const {
		return Get();
	}

	T* operator->() {
		return Get();
	}

	void Release() {
		this->~ptr_t();
	}

	bool IsValid() const {
		return ( object != nullptr ) && ( instances != nullptr );
	}

	T* Get() {
		return ( IsValid() && ( instances->IsFree() == false ) ) ? object : nullptr;
	}

	const T* Get() const {
		return ( IsValid() && ( instances->IsFree() == false ) ) ? object : nullptr;
	}
private:
	T*			object;
	refCount_t*	instances;
};