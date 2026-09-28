#pragma once

#include <cstdint>

#include "Tomos/util/memory/TArena.hh"

namespace Tomos
{
    // Snapshot taken in endFrame(); valid until the next beginFrame().
    struct TFrameMemoryStats
    {
        uint64_t m_arenaBytes     = 0;  // arena bytes used by the frame that just ended
        uint64_t m_arenaAllocs    = 0;
        uint64_t m_arenaPeakBytes = 0;  // peak frame usage since the allocator was created
        uint64_t m_heapProbeBytes = 0;  // heap growth observed inside TOMOS_HEAP_PROBE scopes
        uint64_t m_heapProbeCount = 0;

        void reset()
        {
            m_arenaBytes     = 0;
            m_arenaAllocs    = 0;
            m_arenaPeakBytes = 0;
            m_heapProbeBytes = 0;
            m_heapProbeCount = 0;
        }
    };

    // Per-frame bump allocator. endFrame() rewinds; pointers then invalid.
    // get() asserts outside a tick; current() is nullable for tooling/tests.
    class TFrameAllocator
    {
    public:
        static constexpr size_t g_kDefaultChunkSize = 512 * 1024;

        explicit TFrameAllocator( size_t p_chunkSize = g_kDefaultChunkSize );

        void beginFrame();
        void endFrame();

        [[nodiscard]] TArena&       arena() { return m_arena; }
        [[nodiscard]] const TArena& arena() const { return m_arena; }

        [[nodiscard]] const TFrameMemoryStats& stats() const { return m_stats; }

        void noteHeapProbe( int64_t p_delta );

        static TFrameAllocator* current();
        static void             setCurrent( TFrameAllocator* p_alloc );
        static TFrameAllocator& get();

    private:
        TArena            m_arena;
        TFrameMemoryStats m_stats;
        uint64_t          m_heapProbeBytesAccum = 0;
        uint64_t          m_heapProbeCountAccum = 0;

        static TFrameAllocator* g_sCurrent;
    };

#if TOMOS_DEBUG
    class TScopedHeapProbe
    {
    public:
        explicit TScopedHeapProbe( const char* p_label );
        ~TScopedHeapProbe();

        TScopedHeapProbe( const TScopedHeapProbe& )            = delete;
        TScopedHeapProbe& operator=( const TScopedHeapProbe& ) = delete;

    private:
        const char* m_label;
        size_t      m_before = 0;
    };

#define TOMOS_HEAP_PROBE_CONCAT2( a, b ) a##b
#define TOMOS_HEAP_PROBE_CONCAT( a, b ) TOMOS_HEAP_PROBE_CONCAT2( a, b )
#define TOMOS_HEAP_PROBE( label ) ::Tomos::TScopedHeapProbe TOMOS_HEAP_PROBE_CONCAT( _tomosHeapProbe_, __LINE__ )( label )
#else
#define TOMOS_HEAP_PROBE( label ) ( ( void ) 0 )
#endif
}  // namespace Tomos
