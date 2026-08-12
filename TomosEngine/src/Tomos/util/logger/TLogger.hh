#pragma once

#include <cassert>
#include <chrono>
#include <deque>
#include <format>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <source_location>
#include <sstream>
#include <string>
#include <vector>

namespace Tomos
{
    enum class TLogLevel
    {
        INFO,
        DEBUG,
        WARN,
        ERROR
    };

    enum class TLogDestination
    {
        CONSOLE,
        FILE
    };

    struct TLogEntry
    {
        TLogLevel   m_level = TLogLevel::INFO;
        std::string m_time;
        std::string m_function;
        std::string m_message;
    };

    class TLogger
    {
    public:
        static constexpr size_t k_ringCapacity = 512;

        static TLogger& getInstance();

        template<typename T>
        TLogger& operator<<( const T& p_message )
        {
            std::lock_guard<std::mutex> lock( m_mutex );
            std::ostringstream          oss;
            oss << p_message;
            const std::string chunk = oss.str();
            std::cout << chunk << std::flush;
            m_currentMessage += chunk;
            return *this;
        }

        typedef std::ostream& ( *ManipFn )( std::ostream& );
        TLogger& operator<<( ManipFn p_manip );

        static void beginEntry( TLogLevel p_level = TLogLevel::INFO, std::source_location p_location = std::source_location::current() );
        static void endEntry();

        TLogger( const TLogger& )            = delete;
        TLogger& operator=( const TLogger& ) = delete;

        static std::vector<TLogEntry> snapshotRing();
        static void                   clearRing();

    private:
        TLogger() : m_currentLevel( TLogLevel::INFO ) {}

        ~TLogger() = default;

        void flushCurrentLocked();

        std::mutex m_mutex{};
        TLogLevel  m_currentLevel;

        TLogLevel   m_entryLevel = TLogLevel::INFO;
        std::string m_entryTime;
        std::string m_entryFunction;
        std::string m_currentMessage;
        bool        m_entryOpen = false;

        std::deque<TLogEntry> m_ring;

        std::string getTimestamp();
        std::string getPrefix();
        void        setLogLevel( TLogLevel p_level );
    };

    class TLogLine
    {
    public:
        explicit TLogLine( TLogLevel p_level, std::source_location p_location = std::source_location::current() )
        {
            TLogger::beginEntry( p_level, p_location );
            m_active = true;
        }

        ~TLogLine()
        {
            if ( m_active ) TLogger::endEntry();
        }

        TLogLine( const TLogLine& )            = delete;
        TLogLine& operator=( const TLogLine& ) = delete;

        TLogLine( TLogLine&& p_other ) noexcept : m_active( p_other.m_active ) { p_other.m_active = false; }

        TLogLine& operator=( TLogLine&& p_other ) noexcept
        {
            if ( this != &p_other )
            {
                if ( m_active ) TLogger::endEntry();
                m_active         = p_other.m_active;
                p_other.m_active = false;
            }
            return *this;
        }

        template<typename T>
        TLogLine& operator<<( const T& p_message )
        {
            if ( m_active ) TLogger::getInstance() << p_message;
            return *this;
        }

        TLogLine& operator<<( TLogger::ManipFn p_manip )
        {
            if ( m_active ) TLogger::getInstance() << p_manip;
            return *this;
        }

    private:
        bool m_active = false;
    };

}  // namespace Tomos


#define TLOG_INFO() Tomos::TLogLine( Tomos::TLogLevel::INFO )
#define TLOG_WARN() Tomos::TLogLine( Tomos::TLogLevel::WARN )
#define TLOG_ERROR() Tomos::TLogLine( Tomos::TLogLevel::ERROR )

#ifndef NDEBUG
#define TLOG_DEBUG() Tomos::TLogLine( Tomos::TLogLevel::DEBUG )
#else
#define TLOG_DEBUG() \
    if ( false ) Tomos::TLogLine( Tomos::TLogLevel::DEBUG )
#endif
