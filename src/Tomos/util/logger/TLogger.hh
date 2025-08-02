#pragma once

#include <chrono>
#include <fstream>
#include <iostream>
#include <mutex>
#include <source_location>
#include <string>
#include <cassert>

namespace Tomos
{
    enum class TLogLevel
    {
        DEBUG,
        INFO,
        WARN,
        ERROR
    };

    enum class TLogDestination
    {
        CONSOLE,
        FILE
    };

    class TLogger
    {
    public:
        // Returns the singleton instance
        static TLogger& getInstance();

        // Destructor to close file stream
        ~TLogger();

        // Template operator for logging messages
        template<typename T>
        TLogger& operator<<( const T& p_message )
        {
            if ( m_destination == TLogDestination::FILE && m_fileStream.is_open() )
            {
                m_fileStream << p_message;
            }
            else
            {
                std::cout << p_message;
            }
            return *this;
        }

        typedef std::ostream& ( *ManipFn )( std::ostream& );
        TLogger& operator<<( ManipFn p_manip );

        // Main logging function
        static TLogger& log( TLogLevel p_level = TLogLevel::INFO, std::source_location p_location = std::source_location::current() );

        // Sets the logging destination
        static void reset( TLogDestination p_destination );

    private:
        TLogger() = default;

        TLogger( const TLogger& )            = delete;
        TLogger& operator=( const TLogger& ) = delete;

        std::mutex      m_mutex;
        TLogLevel       m_currentLevel = TLogLevel::INFO;
        TLogDestination m_destination  = TLogDestination::CONSOLE;
        std::ofstream   m_fileStream;

        std::string getPrefix();
        std::string getTimestamp();
        void        setLogLevel( TLogLevel p_level );
    };

    // ======= MACROS FOR LOGGING =======
#define TLOG_INFO() Tomos::TLogger::log( Tomos::TLogLevel::INFO )
#define TLOG_WARN() Tomos::TLogger::log( Tomos::TLogLevel::WARN )
#define TLOG_ERROR() Tomos::TLogger::log( Tomos::TLogLevel::ERROR )

#ifndef NDEBUG
#define TLOG_DEBUG() Tomos::TLogger::log( Tomos::TLogLevel::DEBUG )
#else
#define TLOG_DEBUG() \
    if ( false ) Tomos::TLogger::log( Tomos::TLogLevel::DEBUG )
#endif

#define TLOG_ASSERT_MSG( p_expression, p_message )                      \
    if ( !( p_expression ) )                                            \
    {                                                                   \
        TLOG_ERROR() << "Assertion failed: " << p_message << std::endl; \
        assert( p_expression );                                         \
    }

#define TLOG_ASSERT( p_expression )                      \
    if ( !( p_expression ) )                             \
    {                                                    \
        TLOG_ERROR() << "Assertion failed" << std::endl; \
        assert( p_expression );                          \
    }

}  // namespace Tomos
