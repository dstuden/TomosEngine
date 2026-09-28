#include "Tomos/util/memory/TFrameAllocator.hh"

#include <cassert>

#if defined( __linux__ ) && TOMOS_DEBUG
#include <malloc.h>
#endif

#include "Tomos/util/logger/TLogger.hh"

namespace Tomos
{
    TFrameAllocator* TFrameAllocator::g_sCurrent = nullptr;

    TFrameAllocator::TFrameAllocator( size_t p_chunkSize ) : m_arena( p_chunkSize ) {}

    void TFrameAllocator::beginFrame()
    {
        m_heapProbeBytesAccum = 0;
        m_heapProbeCountAccum = 0;
    }

    void TFrameAllocator::endFrame()
    {
        m_stats.m_arenaBytes     = m_arena.used();
        m_stats.m_arenaAllocs    = m_arena.allocCount();
        m_stats.m_arenaPeakBytes = m_arena.highWater();
        m_stats.m_heapProbeBytes = m_heapProbeBytesAccum;
        m_stats.m_heapProbeCount = m_heapProbeCountAccum;

#if TOMOS_DEBUG
        // Console only on large heap probes; arena stats live in the Performance panel.
        if ( m_stats.m_heapProbeBytes > 64 * 1024 )
        {
            TLOG_DEBUG() << "[FrameMem] arena " << m_stats.m_arenaBytes << "B / " << m_stats.m_arenaAllocs
                         << " allocs (peak " << m_stats.m_arenaPeakBytes << "B); heap probes +"
                         << m_stats.m_heapProbeBytes << "B / " << m_stats.m_heapProbeCount << " scopes";
        }
#endif

        m_arena.rewind();
    }

    void TFrameAllocator::noteHeapProbe( int64_t p_delta )
    {
        if ( p_delta <= 0 ) return;
        m_heapProbeBytesAccum += static_cast<uint64_t>( p_delta );
        ++m_heapProbeCountAccum;
    }

    TFrameAllocator* TFrameAllocator::current() { return g_sCurrent; }

    void TFrameAllocator::setCurrent( TFrameAllocator* p_alloc ) { g_sCurrent = p_alloc; }

    TFrameAllocator& TFrameAllocator::get()
    {
        assert( g_sCurrent != nullptr && "TFrameAllocator::setCurrent not called" );
        return *g_sCurrent;
    }

#if TOMOS_DEBUG
    namespace
    {
        size_t heapUsedBytes()
        {
#if defined( __linux__ )
            return static_cast<size_t>( mallinfo2().uordblks );
#else
            return 0;
#endif
        }
    }  // namespace

    TScopedHeapProbe::TScopedHeapProbe( const char* p_label ) : m_label( p_label ), m_before( heapUsedBytes() ) {}

    TScopedHeapProbe::~TScopedHeapProbe()
    {
        const int64_t delta = static_cast<int64_t>( heapUsedBytes() ) - static_cast<int64_t>( m_before );
        if ( delta > 64 * 1024 ) TLOG_DEBUG() << "[FrameMem] heap +" << delta << "B in scope '" << m_label << "'";
        if ( TFrameAllocator* fa = TFrameAllocator::current() ) fa->noteHeapProbe( delta );
    }
#endif
}  // namespace Tomos
