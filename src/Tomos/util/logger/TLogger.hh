#pragma once

#include <iomanip>
#include <iostream>
#include <mutex>
#include <source_location>
#include <cassert>

namespace Tomos
{
    enum class LogLevel
    {
        INFO,
        DEBUG,
        WARN,
        ERROR
    };

    enum class LogDestination
    {
        CONSOLE,
        FILE
    };

    class TLogger
    {
    public:
        static TLogger& getInstance();

        template<typename T>
        TLogger& operator<<( const T& p_message )
        {
            std::lock_guard<std::mutex> lock( m_mutex );
            std::cout << p_message << std::flush;
            return *this;
        }

        typedef std::ostream& ( *ManipFn )( std::ostream& );
        TLogger&                  operator<<( ManipFn p_manip );

        static TLogger& log( LogLevel             p_level    = LogLevel::INFO,
                            std::source_location p_location = std::source_location::current() );

    private:
        TLogger() :
            m_currentLevel( LogLevel::INFO )
        {
        }

        ~TLogger() = default;

        TLogger( const TLogger& )            = delete;
        TLogger& operator=( const TLogger& ) = delete;

        std::mutex m_mutex{};
        LogLevel   m_currentLevel;

        std::string getTimestamp();
        std::string getPrefix();
        void        setLogLevel( LogLevel p_level );
    };
} // namespace Tomos

// ======= MACROS FOR LOGGING =======

// Always log INFO, WARN, and ERROR, ASSERT
#define TLOG_INFO() Tomos::TLogger::log( Tomos::LogLevel::INFO )
#define TLOG_WARN() Tomos::TLogger::log( Tomos::LogLevel::WARN )
#define TLOG_ERROR() Tomos::TLogger::log( Tomos::LogLevel::ERROR )
#define TLOG_ASSERT_MSG( p_expression, p_message )  \
    if ( !( p_expression ) )        \
    {                               \
        TLOG_ERROR() << p_message << "\n";   \
        assert( p_expression );     \
    }
#define TLOG_ASSERT( p_expression )  \
    if ( !( p_expression ) )        \
    {                               \
        TLOG_ERROR() << "Assertion failed" << "\n"; \
        assert( p_expression );     \
    }

// Only for debug builds
#ifndef NDEBUG
#define TLOG_DEBUG() Tomos::TLogger::log( Tomos::LogLevel::DEBUG )
#else  // real
#define TLOG_DEBUG() \
    if ( false ) Tomos::Logger::log( Tomos::LogLevel::DEBUG )
#endif
