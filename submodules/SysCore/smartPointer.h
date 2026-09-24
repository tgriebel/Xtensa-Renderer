#pragma once

#include "refCounter.h"
#include <assert.h>
#include <cstdint>

template<typename T>
class ptr_t
{
public:
	ptr_t()
	{
		this->object = nullptr;
		this->instances = nullptr;
	};

	ptr_t( const T& obj )
	{
		this->object = new T( obj );
		this->instances = new refCount_t( 1 );
	}

	ptr_t( const ptr_t& handle )
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

	~ptr_t()
	{
		if ( IsValid() )
		{
			instances->Release();
			if ( instances->IsFree() )
			{
				delete instances;
				delete object;
			}
			instances = nullptr;
			object = nullptr;
		}
	}

	ptr_t& operator=( const ptr_t& handle )
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

	bool operator==( const ptr_t& rhs ) const {
		return ( object == rhs.object );
	}

	bool operator!=( const ptr_t& rhs ) const {
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