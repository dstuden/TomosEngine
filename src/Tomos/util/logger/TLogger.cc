#include "TLogger.hh"

#include <iomanip>
#include <iostream>
#include <sstream>

#include "Tomos/util/conf/TConfig.hh"
#include "Tomos/util/time/TTime.hh"

namespace Tomos
{
    TLogger& TLogger::getInstance()
    {
        static TLogger instance;
        return instance;
    }

    TLogger::~TLogger()
    {
        if ( m_fileStream.is_open() )
        {
            m_fileStream.close();
        }
    }

    TLogger& TLogger::operator<<( ManipFn p_manip )
    {
        if ( m_destination == TLogDestination::FILE && m_fileStream.is_open() )
        {
            p_manip( m_fileStream );
        }
        else
        {
            p_manip( std::cout );
        }
        return *this;
    }

    TLogger& TLogger::log( TLogLevel p_level, const std::source_location p_location )
    {
        TLogger&                    instance = getInstance();
        std::lock_guard<std::mutex> lock( instance.m_mutex );

        instance.setLogLevel( p_level );
        std::string prefix = instance.getPrefix();

        if ( instance.m_destination == TLogDestination::FILE && instance.m_fileStream.is_open() )
        {
            instance.m_fileStream << "\n" << prefix << " " << p_location.function_name() << ": ";
        }
        else
        {
            // Reset color for new line and apply color for prefix
            std::cout << "\n" << "\033[0m" << prefix << " " << p_location.function_name() << ": ";
        }

        return instance;
    }

    void TLogger::setLogLevel( TLogLevel p_level ) { m_currentLevel = p_level; }

    std::string TLogger::getPrefix()
    {
        std::string prefix;
        std::string timestamp = getTimestamp();

        switch ( m_currentLevel )
        {
            case TLogLevel::DEBUG:
                prefix = m_destination == TLogDestination::CONSOLE ? "\033[32m" + timestamp + " [DEBUG] " : timestamp + " [DEBUG] ";
                break;
            case TLogLevel::INFO:
                prefix = m_destination == TLogDestination::CONSOLE ? "\033[36m" + timestamp + " [INFO]  " : timestamp + " [INFO]  ";
                break;
            case TLogLevel::WARN:
                prefix = m_destination == TLogDestination::CONSOLE ? "\033[33m" + timestamp + " [WARN]  " : timestamp + " [WARN]  ";
                break;
            case TLogLevel::ERROR:
                prefix = m_destination == TLogDestination::CONSOLE ? "\033[31m" + timestamp + " [ERROR] " : timestamp + " [ERROR] ";
                break;
        }

        return prefix;
    }

    std::string TLogger::getTimestamp()
    {
        auto now      = std::chrono::system_clock::now();
        auto timeTNow = std::chrono::system_clock::to_time_t( now );
        auto ms       = std::chrono::duration_cast<std::chrono::milliseconds>( now.time_since_epoch() ) % 1000;

        std::tm localTm = *std::localtime( &timeTNow );

        std::ostringstream oss;
        oss << "[" << std::put_time( &localTm, "%Y-%m-%d %H:%M:%S" ) << "." << std::setfill( '0' ) << std::setw( 3 ) << ms.count() << "]";
        return oss.str();
    }

    void TLogger::reset( TLogDestination p_destination )
    {
        TLogger&                    instance = getInstance();
        std::lock_guard<std::mutex> lock( instance.m_mutex );
        if ( instance.m_destination == p_destination )
        {
            return;
        }

        instance.m_destination = p_destination;
        if ( instance.m_destination == TLogDestination::FILE )
        {
            std::string logDir   = Global::config.get<std::string>( "logDir" );
            std::string fileName = "tomos_log_" + TTime::getCurrentDateTime( "%Y_%m_%d_%H_%M_%S" ) + ".log";

            if ( !std::filesystem::exists( logDir ) )
            {
                std::filesystem::create_directories( logDir );
            }

            instance.m_fileStream.open( std::filesystem::path( logDir ) / fileName, std::ios_base::app );
            if ( !instance.m_fileStream.is_open() )
            {
                // Handle error: fall back to console logging
                instance.m_destination = TLogDestination::CONSOLE;
                TLOG_ERROR() << "Failed to open log file: " << fileName << ". Falling back to console.\n";
            }
        }
        else if ( instance.m_fileStream.is_open() )
        {
            instance.m_fileStream.close();
        }
    }
}  // namespace Tomos
