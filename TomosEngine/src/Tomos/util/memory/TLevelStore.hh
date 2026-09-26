#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#include "Tomos/systems/TComponent.hh"
#include "Tomos/util/memory/TArena.hh"
#include "Tomos/util/memory/THandle.hh"

namespace Tomos
{
    class TSceneNode;

    // Per-scene ownership: paged node pool + level-arena / heap-adopted components.
    class TLevelStore
    {
    public:
        TLevelStore();
        ~TLevelStore();

        TLevelStore( const TLevelStore& )            = delete;
        TLevelStore& operator=( const TLevelStore& ) = delete;

        TNodeHandle createNode( std::string p_name = "<unnamed>" );

        void destroyNode( TNodeHandle p_handle );

        [[nodiscard]] TSceneNode*       getNode( TNodeHandle p_handle );
        [[nodiscard]] const TSceneNode* getNode( TNodeHandle p_handle ) const;

        template<typename T, typename... Args>
        T* createComponent( Args&&... p_args )
        {
            static_assert( std::is_base_of_v<TComponent, T>, "T must derive from TComponent" );
            void* mem = allocComponentSlot( sizeof( T ), alignof( T ) );
            T*    obj = new ( mem ) T( std::forward<Args>( p_args )... );
            m_components.emplace( obj, TComponentEntry{ []( TComponent* p ) { static_cast<T*>( p )->~T(); }, sizeof( T ), alignof( T ), false } );
            return obj;
        }

        TComponent* adoptComponent( std::unique_ptr<TComponent> p_comp );

        void destroyComponent( TComponent* p_component );

        void clear();

        [[nodiscard]] size_t nodeCount() const;
        [[nodiscard]] size_t componentCount() const { return m_components.size(); }

        // Bumps on node create/destroy and addChild/removeChild.
        [[nodiscard]] uint64_t topologyVersion() const { return m_topologyVersion; }

    private:
        friend class TSceneNode;

        struct TComponentEntry
        {
            void ( *m_destroy )( TComponent* ) = nullptr;
            uint32_t m_size  = 0;
            uint32_t m_align = 0;
            bool     m_heap  = false;
        };

        void* allocComponentSlot( size_t p_size, size_t p_align );
        void  recycleComponentSlot( void* p_mem, size_t p_size, size_t p_align );
        void  bumpTopology() { ++m_topologyVersion; }

        struct TImpl;
        std::unique_ptr<TImpl>                           m_impl;
        TArena                                           m_componentArena{ 256 * 1024 };
        std::unordered_map<TComponent*, TComponentEntry> m_components;
        std::unordered_map<uint64_t, std::vector<void*>> m_freeSlots;  // (size, align) → recycled arena slots
        uint64_t                                         m_topologyVersion = 1;
    };
}  // namespace Tomos
