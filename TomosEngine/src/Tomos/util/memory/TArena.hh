#pragma once

#include <cstddef>
#include <cstdint>
#include <new>
#include <utility>
#include <vector>

namespace Tomos
{
    // Bump (linear) allocator. No per-allocation free — only rewind / reset.
    // Never store pointers from a frame arena past rewind.
    class TArena
    {
    public:
        static constexpr size_t g_kDefaultChunkSize = 256 * 1024;
        static constexpr size_t g_kDefaultAlign     = alignof( std::max_align_t );

        explicit TArena( size_t p_chunkSize = g_kDefaultChunkSize );
        ~TArena();

        TArena( const TArena& )            = delete;
        TArena& operator=( const TArena& ) = delete;
        TArena( TArena&& ) noexcept;
        TArena& operator=( TArena&& ) noexcept;

        [[nodiscard]] void* alloc( size_t p_size, size_t p_align = g_kDefaultAlign );

        template<typename T>
        [[nodiscard]] T* alloc( size_t p_count = 1 )
        {
            return static_cast<T*>( alloc( sizeof( T ) * p_count, alignof( T ) ) );
        }

        template<typename T, typename... Args>
        [[nodiscard]] T* create( Args&&... p_args )
        {
            void* mem = alloc( sizeof( T ), alignof( T ) );
            return new ( mem ) T( std::forward<Args>( p_args )... );
        }

        void rewind();
        void reset();

        [[nodiscard]] size_t used() const { return m_used; }
        [[nodiscard]] size_t capacity() const { return m_capacity; }
        [[nodiscard]] size_t allocCount() const { return m_allocCount; }
        [[nodiscard]] size_t highWater() const { return m_highWater; }

    private:
        struct TChunk
        {
            uint8_t* m_data = nullptr;
            size_t   m_size = 0;
        };

        void ensureChunk( size_t p_need );

        size_t              m_chunkSize  = g_kDefaultChunkSize;
        std::vector<TChunk> m_chunks;
        size_t              m_chunkIndex = 0;
        size_t              m_offset     = 0;
        size_t              m_used       = 0;
        size_t              m_capacity   = 0;
        size_t              m_allocCount = 0;
        size_t              m_highWater  = 0;
    };
}  // namespace Tomos
