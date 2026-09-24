#pragma once
#include <atomic>
#include <chrono>
#include <thread>

class SpinLock
{
public:
	SpinLock() {
		lock.store( false );
	}

	void Lock() {
		bool expected = false;
		while ( lock.compare_exchange_strong( expected, true ) == false ) {
			std::this_thread::sleep_for( std::chrono::seconds( 1 ) );
		}
	}
	void Unlock() {
		if ( lock.load() ) {
			lock.store( false );
		}
	}
private:
	std::atomic<bool> lock;
};