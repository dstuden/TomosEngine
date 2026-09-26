#include "Tomos/util/memory/TArena.hh"

#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <cstring>

namespace Tomos
{
    namespace
    {
        size_t alignUp( size_t p_value, size_t p_align )
        {
            assert( p_align > 0 && ( p_align & ( p_align - 1 ) ) == 0 );
            return ( p_value + p_align - 1 ) & ~( p_align - 1 );
        }
    }  // namespace

    TArena::TArena( size_t p_chunkSize ) : m_chunkSize( std::max( p_chunkSize, size_t{ 4096 } ) ) {}

    TArena::~TArena() { reset(); }

    TArena::TArena( TArena&& p_o ) noexcept
        : m_chunkSize( p_o.m_chunkSize ),
          m_chunks( std::move( p_o.m_chunks ) ),
          m_chunkIndex( p_o.m_chunkIndex ),
          m_offset( p_o.m_offset ),
          m_used( p_o.m_used ),
          m_capacity( p_o.m_capacity ),
          m_allocCount( p_o.m_allocCount ),
          m_highWater( p_o.m_highWater )
    {
        p_o.m_chunkIndex = 0;
        p_o.m_offset     = 0;
        p_o.m_used       = 0;
        p_o.m_capacity   = 0;
        p_o.m_allocCount = 0;
        p_o.m_highWater  = 0;
    }

    TArena& TArena::operator=( TArena&& p_o ) noexcept
    {
        if ( this == &p_o ) return *this;
        reset();
        m_chunkSize      = p_o.m_chunkSize;
        m_chunks         = std::move( p_o.m_chunks );
        m_chunkIndex     = p_o.m_chunkIndex;
        m_offset         = p_o.m_offset;
        m_used           = p_o.m_used;
        m_capacity       = p_o.m_capacity;
        m_allocCount     = p_o.m_allocCount;
        m_highWater      = p_o.m_highWater;
        p_o.m_chunkIndex = 0;
        p_o.m_offset     = 0;
        p_o.m_used       = 0;
        p_o.m_capacity   = 0;
        p_o.m_allocCount = 0;
        p_o.m_highWater  = 0;
        return *this;
    }

    void TArena::ensureChunk( size_t p_need )
    {
        if ( m_chunkIndex < m_chunks.size() )
        {
            if ( m_offset + p_need <= m_chunks[ m_chunkIndex ].m_size ) return;
            ++m_chunkIndex;
            m_offset = 0;
            if ( m_chunkIndex < m_chunks.size() && p_need <= m_chunks[ m_chunkIndex ].m_size ) return;
        }

        const size_t chunkBytes = std::max( m_chunkSize, p_need );
        auto*        data       = static_cast<uint8_t*>( std::malloc( chunkBytes ) );
        if ( data == nullptr ) throw std::bad_alloc();

        m_chunks.push_back( TChunk{ data, chunkBytes } );
        m_chunkIndex = m_chunks.size() - 1;
        m_offset     = 0;
        m_capacity += chunkBytes;
    }

    void* TArena::alloc( size_t p_size, size_t p_align )
    {
        if ( p_size == 0 ) p_size = 1;
        if ( p_align == 0 ) p_align = g_kDefaultAlign;

        if ( m_chunks.empty() ) ensureChunk( p_size + p_align );

        // Align absolute address (malloc only guarantees max_align_t).
        auto alignedOffset = [ & ]() -> size_t
        {
            const auto base = reinterpret_cast<uintptr_t>( m_chunks[ m_chunkIndex ].m_data );
            return static_cast<size_t>( alignUp( base + m_offset, p_align ) - base );
        };

        size_t aligned = alignedOffset();
        if ( aligned + p_size > m_chunks[ m_chunkIndex ].m_size )
        {
            ensureChunk( p_size + p_align );
            aligned = alignedOffset();
        }

        void* ptr = m_chunks[ m_chunkIndex ].m_data + aligned;
        m_offset  = aligned + p_size;
        m_used += p_size;
        ++m_allocCount;
        m_highWater = std::max( m_highWater, m_used );
        return ptr;
    }

    void TArena::rewind()
    {
#if TOMOS_DEBUG
        // Poison bytes handed out this cycle.
        for ( size_t i = 0; i < m_chunks.size() && i <= m_chunkIndex; ++i )
        {
            const size_t n = ( i == m_chunkIndex ) ? std::min( m_offset, m_chunks[ i ].m_size ) : m_chunks[ i ].m_size;
            std::memset( m_chunks[ i ].m_data, 0xDD, n );
        }
#endif
        m_chunkIndex = 0;
        m_offset     = 0;
        m_used       = 0;
        m_allocCount = 0;
    }

    void TArena::reset()
    {
        for ( TChunk& c : m_chunks ) std::free( c.m_data );
        m_chunks.clear();
        m_chunkIndex = 0;
        m_offset     = 0;
        m_used       = 0;
        m_capacity   = 0;
        m_allocCount = 0;
        m_highWater  = 0;
    }
}  // namespace Tomos
