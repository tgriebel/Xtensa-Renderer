#include "timer.h"

#include <iostream>
#include <assert.h>
#include <chrono>
using namespace std::chrono;

namespace SysCore
{

uint64_t Timer::Convert( const std::chrono::nanoseconds& start, const std::chrono::nanoseconds& end ) const
{
	if( m_precision == timerPrecision_t::NANOSECOND ) {
		return duration_cast< nanoseconds >( end - start ).count();
	}
	if( m_precision == timerPrecision_t::MICROSECOND ) {
		return duration_cast< microseconds >( end - start ).count();
	}
	if( m_precision == timerPrecision_t::MILLISECOND ) {
		return duration_cast< milliseconds >( end - start ).count();
	}
	if( m_precision == timerPrecision_t::SECOND ) {
		return duration_cast< seconds >( end - start ).count();
	}
	return duration_cast< nanoseconds >( end - start ).count();
}


timerPrecision_t Timer::GetPrecision() const
{
	return m_precision;
}


std::string	Timer::GetLabel() const
{
	return m_label;
}


void Timer::Start()
{
	m_startTime = duration_cast< nanoseconds >( steady_clock::now().time_since_epoch() );
	m_endTime = m_startTime;
}


void Timer::Stop()
{
	m_endTime = duration_cast< nanoseconds >( steady_clock::now().time_since_epoch() );
}


uint64_t Timer::GetElapsed() const
{
	return Convert( m_startTime, m_endTime );
}


void Timer::SetPrecision( const timerPrecision_t precision )
{
	m_precision = precision;
}


uint64_t Timer::GetCurrentElapsed() const
{
	const nanoseconds time = duration_cast< nanoseconds >( steady_clock::now().time_since_epoch() );
	return Convert( m_startTime, time );
}


void TimerPrint( const Timer* timer )
{
	assert( timer != nullptr );

	const timerPrecision_t precision = timer->GetPrecision();
	std::string precisionStr = "";
	switch( precision ) {
	case timerPrecision_t::NANOSECOND: precisionStr = "ns"; break;
	case timerPrecision_t::MICROSECOND: precisionStr = "us"; break;
	case timerPrecision_t::MILLISECOND: precisionStr = "ms"; break;
	case timerPrecision_t::SECOND: precisionStr = "s"; break;
	}

	std::cout << "Timer(" << timer->GetLabel() << "): " << timer->GetCurrentElapsed() << precisionStr << std::endl;
}
}
