#pragma once

#include <cassert>
#include <cstddef>
#include <memory>
#include <utility>
#include <vector>

#include "Tomos/util/memory/THandle.hh"

namespace Tomos
{
    // Paged freelist pool; pages never relocate, so T* is stable until destroy/clear.
    template<typename T, typename Tag = T, size_t PageSize = 64>
    class TPool
    {
        static_assert( PageSize > 0, "PageSize must be > 0" );

    public:
        using Handle = THandle<Tag>;

        TPool() = default;
        ~TPool() { clear(); }

        TPool( const TPool& )            = delete;
        TPool& operator=( const TPool& ) = delete;

        template<typename... Args>
        Handle create( Args&&... p_args )
        {
            uint32_t index;
            if ( !m_free.empty() )
            {
                index = m_free.back();
                m_free.pop_back();
            }
            else
            {
                index = m_count;
                if ( index / PageSize >= m_pages.size() ) m_pages.push_back( std::make_unique<TSlot[]>( PageSize ) );
                ++m_count;
                slotAt( index ).m_generation = 1;
            }

            TSlot& slot = slotAt( index );
            assert( !slot.m_live );
            new ( &slot.m_storage ) T( std::forward<Args>( p_args )... );
            slot.m_live = true;

            Handle h;
            h.m_index      = index;
            h.m_generation = slot.m_generation;
            return h;
        }

        void destroy( Handle p_handle )
        {
            T* obj = get( p_handle );
            if ( obj == nullptr ) return;
            TSlot& slot = slotAt( p_handle.m_index );
            obj->~T();
            slot.m_live = false;
            ++slot.m_generation;
            if ( slot.m_generation == 0 ) slot.m_generation = 1;
            m_free.push_back( p_handle.m_index );
        }

        [[nodiscard]] T* get( Handle p_handle )
        {
            if ( !p_handle.valid() || p_handle.m_index >= m_count ) return nullptr;
            TSlot& slot = slotAt( p_handle.m_index );
            if ( !slot.m_live || slot.m_generation != p_handle.m_generation ) return nullptr;
            return reinterpret_cast<T*>( &slot.m_storage );
        }

        [[nodiscard]] const T* get( Handle p_handle ) const { return const_cast<TPool*>( this )->get( p_handle ); }

        void clear()
        {
            for ( uint32_t i = 0; i < m_count; ++i )
            {
                TSlot& slot = slotAt( i );
                if ( !slot.m_live ) continue;
                reinterpret_cast<T*>( &slot.m_storage )->~T();
                slot.m_live = false;
            }
            m_pages.clear();
            m_free.clear();
            m_count = 0;
        }

        [[nodiscard]] size_t liveCount() const { return static_cast<size_t>( m_count ) - m_free.size(); }

        // Visit live objects. p_fn must not create/destroy pool entries.
        template<typename Fn>
        void forEach( Fn&& p_fn )
        {
            for ( uint32_t i = 0; i < m_count; ++i )
            {
                TSlot& slot = slotAt( i );
                if ( !slot.m_live ) continue;
                Handle h;
                h.m_index      = i;
                h.m_generation = slot.m_generation;
                p_fn( h, *reinterpret_cast<T*>( &slot.m_storage ) );
            }
        }

    private:
        struct TSlot
        {
            alignas( T ) unsigned char m_storage[ sizeof( T ) ];
            uint32_t m_generation = 0;
            bool     m_live       = false;
        };

        [[nodiscard]] TSlot&       slotAt( uint32_t p_index ) { return m_pages[ p_index / PageSize ][ p_index % PageSize ]; }
        [[nodiscard]] const TSlot& slotAt( uint32_t p_index ) const { return m_pages[ p_index / PageSize ][ p_index % PageSize ]; }

        std::vector<std::unique_ptr<TSlot[]>> m_pages;
        std::vector<uint32_t>                 m_free;
        uint32_t                              m_count = 0;
    };
}  // namespace Tomos
