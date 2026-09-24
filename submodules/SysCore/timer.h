#pragma once

#include <string>
#include <chrono>

namespace SysCore
{

enum class timerPrecision_t : uint32_t
{
	NANOSECOND,
	MICROSECOND,
	MILLISECOND,
	SECOND,
};


class Timer
{
protected:
	std::chrono::nanoseconds	m_startTime;
	std::chrono::nanoseconds	m_endTime;
	std::string					m_label;
	timerPrecision_t			m_precision;

	uint64_t Convert( const std::chrono::nanoseconds& start, const std::chrono::nanoseconds& end ) const;

public:

	Timer()
	{
		m_label = "";
		m_precision = timerPrecision_t::MILLISECOND;
		Start();
	}

	Timer( std::string label, const timerPrecision_t precision = timerPrecision_t::MILLISECOND )
	{
		m_label = label;
		m_precision = precision;
		Start();
	}

	void				Start();
	void				Stop();
	void				SetPrecision( const timerPrecision_t precision );

	[[nodiscard]]
	timerPrecision_t	GetPrecision() const;

	[[nodiscard]]
	std::string			GetLabel() const;

	[[nodiscard]]
	uint64_t			GetElapsed() const;

	[[nodiscard]]
	uint64_t			GetCurrentElapsed() const;
};


typedef void( *scopedTimerLogCallback_t )( const Timer* );

class ScopedLogTimer : public Timer
{
private:
	scopedTimerLogCallback_t m_callback;

public:
	ScopedLogTimer( std::string label, const timerPrecision_t precision = timerPrecision_t::MILLISECOND, scopedTimerLogCallback_t callback = nullptr )
	{
		m_label = label;
		m_callback = callback;
		m_precision = precision;
		Start();
	}

	~ScopedLogTimer()
	{
		if( m_callback != nullptr ) {
			( *m_callback )( this );
		}
	}
};

void TimerPrint( const Timer* timer );
}
#define SCOPED_TIMER_PRINT( label, timeUnit ) SysCore::ScopedLogTimer scopedTimer_##label( #label, SysCore::timerPrecision_t::timeUnit, &SysCore::TimerPrint );
