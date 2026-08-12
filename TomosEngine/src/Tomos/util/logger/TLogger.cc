#include "TLogger.hh"

#include <source_location>

namespace Tomos
{
    TLogger& TLogger::getInstance()
    {
        static TLogger instance;
        return instance;
    }

    void TLogger::flushCurrentLocked()
    {
        if ( !m_entryOpen ) return;
        TLogEntry entry;
        entry.m_level    = m_entryLevel;
        entry.m_time     = m_entryTime;
        entry.m_function = m_entryFunction;
        entry.m_message  = m_currentMessage;
        m_ring.push_back( std::move( entry ) );
        while ( m_ring.size() > k_ringCapacity ) m_ring.pop_front();
        m_currentMessage.clear();
        m_entryOpen = false;
    }

    TLogger& TLogger::operator<<( ManipFn p_manip )
    {
        std::lock_guard<std::mutex> lock( m_mutex );
        p_manip( std::cout );
        if ( p_manip == static_cast<ManipFn>( std::endl ) ) flushCurrentLocked();
        return *this;
    }

    void TLogger::beginEntry( TLogLevel p_level, const std::source_location p_location )
    {
        std::cout << "\033[0m";

        TLogger&                    instance = getInstance();
        std::lock_guard<std::mutex> lock( instance.m_mutex );
        instance.flushCurrentLocked();
        instance.setLogLevel( p_level );
        instance.m_entryLevel    = p_level;
        instance.m_entryTime     = instance.getTimestamp();
        instance.m_entryFunction = p_location.function_name();
        instance.m_currentMessage.clear();
        instance.m_entryOpen = true;
        std::cout << std::endl << instance.getPrefix() << " " << p_location.function_name() << ": ";
    }

    void TLogger::endEntry()
    {
        TLogger&                    instance = getInstance();
        std::lock_guard<std::mutex> lock( instance.m_mutex );
        instance.flushCurrentLocked();
    }

    void TLogger::setLogLevel( TLogLevel p_level ) { m_currentLevel = p_level; }

    std::string TLogger::getPrefix()
    {
        std::string prefix = getTimestamp();
        switch ( m_currentLevel )
        {
            case TLogLevel::DEBUG:
                return "\033[32m" + prefix + "[DEBUG] ";
            case TLogLevel::INFO:
                return "\033[36m" + prefix + "[INFO]  ";
            case TLogLevel::WARN:
                return "\033[33m" + prefix + "[WARN]  ";
            case TLogLevel::ERROR:
                return "\033[31m" + prefix + "[ERROR] ";
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
        oss << "[" << std::put_time( &local_tm, "%Y-%m-%d %H:%M:%S" ) << "." << std::setfill( '0' ) << std::setw( 3 ) << ms.count() << "] ";
        return oss.str();
    }

    std::vector<TLogEntry> TLogger::snapshotRing()
    {
        TLogger&                    instance = getInstance();
        std::lock_guard<std::mutex> lock( instance.m_mutex );
        instance.flushCurrentLocked();
        return { instance.m_ring.begin(), instance.m_ring.end() };
    }

    void TLogger::clearRing()
    {
        TLogger&                    instance = getInstance();
        std::lock_guard<std::mutex> lock( instance.m_mutex );
        instance.m_ring.clear();
        instance.m_currentMessage.clear();
        instance.m_entryOpen = false;
    }
}  // namespace Tomos
