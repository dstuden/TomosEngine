//
// Created by dstuden on 1/19/25.
//

#include <source_location>

#include "TLogger.hh"

namespace Tomos
{
    TLogger& TLogger::getInstance()
    {
        static TLogger instance;
        return instance;
    }

    TLogger& TLogger::operator<<( ManipFn p_manip )
    {
        std::lock_guard<std::mutex> lock( m_mutex );
        p_manip( std::cout );
        return *this;
    }

    TLogger& TLogger::log( LogLevel p_level, const std::source_location p_location )
    {
        std::cout << "\033[0m";

        TLogger& instance = getInstance();

        std::lock_guard<std::mutex> lock( instance.m_mutex );
        instance.setLogLevel( p_level );
        std::cout << std::endl << instance.getPrefix() << " " << p_location.function_name() << ": ";
        return instance;
    }

    void TLogger::setLogLevel( LogLevel p_level ) { m_currentLevel = p_level; }

    std::string TLogger::getPrefix()
    {
        std::string prefix = getTimestamp();
        switch ( m_currentLevel )
        {
            case LogLevel::DEBUG:
                return "\033[32m" + getTimestamp() + "[DEBUG] ";
            case LogLevel::INFO:
                return "\033[36m" + getTimestamp() + "[INFO]  ";
            case LogLevel::WARN:
                return "\033[33m" + getTimestamp() + "[WARN]  ";
            case LogLevel::ERROR:
                return "\033[31m" + getTimestamp() + "[ERROR] ";
            default:
                break;
        }

        return prefix;
    }

    std::string TLogger::getTimestamp()
    {
        auto now        = std::chrono::system_clock::now();
        auto time_t_now = std::chrono::system_clock::to_time_t( now );
        auto ms         = std::chrono::duration_cast<std::chrono::milliseconds>( now.time_since_epoch() ) % 1000;

        std::tm local_tm = *std::localtime( &time_t_now );

        std::ostringstream oss;
        oss << "[" << std::put_time( &local_tm, "%Y-%m-%d %H:%M:%S" ) << "." << std::setfill( '0' ) << std::setw( 3 )
                << ms.count() << "] ";
        return oss.str();
    }
} // namespace Tomos
