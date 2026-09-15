#include "log.h"

#include <atomic>
#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include <syscore/systemUtils.h>

// Log system uses stack for storing system-specific tags (e.g. "Renderer")
static thread_local std::vector<const char*> s_systemStack;

static const char* CurrentSystem()
{
	return s_systemStack.empty() ? "App" : s_systemStack.back();
}

LogScopeSystem::LogScopeSystem( const char* system )
{
	s_systemStack.push_back( system );
}

LogScopeSystem::~LogScopeSystem()
{
	s_systemStack.pop_back();
}


static const char* SeverityName( logSeverity_t severity )
{
	switch ( severity )
	{
		case logSeverity_t::Verbose:	return "Verbose";
		case logSeverity_t::Info:		return "Info";
		case logSeverity_t::Warning:	return "Warning";
		case logSeverity_t::Error:		return "Error";
		default:						return "Unknown";
	}
}


struct logSlot_t
{
	logRecord_t			record;
	std::atomic<bool>	ready{ false };
};


static constexpr uint32_t		LogBufferCapacity = 4096;

static logSlot_t				s_slots[ LogBufferCapacity ];
static std::atomic<uint64_t>	s_writeIndex{ 0 };
static std::atomic<bool>		s_shutdown{ false };

static std::mutex				s_pathMutex;
static std::string				s_logFilePath = "log.txt";
static std::atomic<bool>		s_pathChanged{ true };


static logRecord_t& AssignSlot( logSlot_t*& outSlot )
{
	const uint64_t index = s_writeIndex.fetch_add( 1, std::memory_order_relaxed );
	outSlot = &s_slots[ index % LogBufferCapacity ];
	return outSlot->record;
}


static void ProcessRecord( const logRecord_t& rec, std::ofstream& file )
{
	std::ostream& outputStream = ( rec.severity >= logSeverity_t::Warning ) ? std::cerr : std::cout;

	const bool isVulkan = SysCore::HasPrefix( rec.system, "Vulkan" );

	outputStream << ( isVulkan ? ">>[" : "[" ) << rec.system << "][";
	outputStream << SeverityName( rec.severity ) << "] " << rec.message << "\n";

	if ( file.is_open() )
	{
		file << "[" << rec.system << "][" << SeverityName( rec.severity ) << "] " << rec.message << "\n";

		if ( rec.severity > logSeverity_t::Warning ) {
			file.flush();
		}
	}
}


static void LogThreadMain()
{
	std::ofstream file;

	uint64_t readIndex = 0;
	for ( ;; )
	{
		if ( s_pathChanged.exchange( false, std::memory_order_acq_rel ) )
		{
			std::string path;
			{
				std::lock_guard<std::mutex> lock( s_pathMutex );
				path = s_logFilePath;
			}
			file.close();
			file.open( path, std::ios::app );
		}

		logSlot_t& slot = s_slots[ readIndex % LogBufferCapacity ];
		if ( slot.ready.load( std::memory_order_acquire ) )
		{
			ProcessRecord( slot.record, file );
			slot.ready.store( false, std::memory_order_release );
			++readIndex;
			continue;
		}

		if ( s_shutdown.load( std::memory_order_acquire ) ) {
			return;
		}

		std::this_thread::sleep_for( std::chrono::milliseconds( 1 ) );
	}
}


class LogThread
{
	std::thread thread;

public:
	LogThread() : thread( LogThreadMain ) {}

	~LogThread()
	{
		s_shutdown.store( true, std::memory_order_release );
		if ( thread.joinable() ) {
			thread.join();
		}
	}
};

static LogThread s_logThread;


void SetLogFilePath( const char* path )
{
	{
		std::lock_guard<std::mutex> lock( s_pathMutex );
		s_logFilePath = path;
	}
	s_pathChanged.store( true, std::memory_order_release );
}


void LogMsgV( const char* system, logSeverity_t severity, const char* fmt, va_list args )
{
	logSlot_t* slot;
	logRecord_t& rec = AssignSlot( slot );

	rec.system = system;
	rec.severity = severity;
	vsnprintf( rec.message, LogMessageMaxLength, fmt, args );

	slot->ready.store( true, std::memory_order_release );
}


void LogMsg( const char* system, logSeverity_t severity, const char* fmt, ... )
{
	va_list args;
	va_start( args, fmt );
	LogMsgV( system, severity, fmt, args );
	va_end( args );
}


void LogMsg( const char* system, const char* fmt, ... )
{
	va_list args;
	va_start( args, fmt );
	LogMsgV( system, logSeverity_t::Info, fmt, args );
	va_end( args );
}


void LogMsg( logSeverity_t severity, const char* fmt, ... )
{
	va_list args;
	va_start( args, fmt );
	LogMsgV( CurrentSystem(), severity, fmt, args );
	va_end( args );
}


void LogMsg( const char* fmt, ... )
{
	va_list args;
	va_start( args, fmt );
	LogMsgV( CurrentSystem(), logSeverity_t::Info, fmt, args );
	va_end( args );
}
