#pragma once

#include <limits>
#include <new>
#include <vector>

#include "Tomos/util/memory/TArena.hh"

namespace Tomos
{
    // Arena-backed STL allocator; deallocate is a no-op.
    template<typename T>
    class TArenaAllocator
    {
    public:
        using value_type = T;

        TArenaAllocator() noexcept : m_arena( nullptr ) {}
        explicit TArenaAllocator( TArena& p_arena ) noexcept : m_arena( &p_arena ) {}

        template<typename U>
        TArenaAllocator( const TArenaAllocator<U>& p_other ) noexcept : m_arena( p_other.arena() )
        {
        }

        [[nodiscard]] T* allocate( size_t p_n )
        {
            if ( m_arena == nullptr ) throw std::bad_alloc();
            if ( p_n > std::numeric_limits<size_t>::max() / sizeof( T ) ) throw std::bad_alloc();
            return static_cast<T*>( m_arena->alloc( p_n * sizeof( T ), alignof( T ) ) );
        }

        void deallocate( T*, size_t ) noexcept {}

        [[nodiscard]] TArena* arena() const noexcept { return m_arena; }

        template<typename U>
        bool operator==( const TArenaAllocator<U>& p_o ) const noexcept
        {
            return m_arena == p_o.arena();
        }

        template<typename U>
        bool operator!=( const TArenaAllocator<U>& p_o ) const noexcept
        {
            return !( *this == p_o );
        }

        template<typename U>
        struct rebind
        {
            using other = TArenaAllocator<U>;
        };

    private:
        TArena* m_arena;
    };

    template<typename T>
    using TArenaVector = std::vector<T, TArenaAllocator<T>>;
}  // namespace Tomos
