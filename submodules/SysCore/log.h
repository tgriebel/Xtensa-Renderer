#pragma once

#include <cstdarg>
#include <cstdint>


enum class logSeverity_t : uint8_t
{
	Verbose,
	Info,
	Warning,
	Error,
};

constexpr uint32_t LogMessageMaxLength = 256;


struct logRecord_t
{
	const char*		system;
	logSeverity_t	severity;
	char			message[ LogMessageMaxLength ];
};

// Opens a channel, within in a scope, for a given system.
class LogScopeSystem
{
public:
	explicit LogScopeSystem( const char* system );
	~LogScopeSystem();

	LogScopeSystem( const LogScopeSystem& ) = delete;
	LogScopeSystem& operator=( const LogScopeSystem& ) = delete;
};

#define LOG_SCOPE_SYSTEM( sys ) LogScopeSystem logScopeSystem_##sys( #sys )

void SetLogFilePath( const char* path );

void LogMsgV( const char* system, logSeverity_t severity, const char* fmt, va_list args );

void LogMsg( const char* system, logSeverity_t severity, const char* fmt, ... );
void LogMsg( logSeverity_t severity, const char* fmt, ... );	// system comes from the active LOG_SCOPE_SYSTEM
void LogMsg( const char* fmt, ... );							// system from LOG_SCOPE_SYSTEM, severity defaults to Info
