#include "Tomos/util/memory/TLevelStore.hh"

#include <algorithm>

#include "Tomos/core/scene/TSceneNode.hh"
#include "Tomos/util/memory/TPool.hh"

namespace Tomos
{
    namespace
    {
        uint64_t slotKey( size_t p_size, size_t p_align ) { return ( static_cast<uint64_t>( p_size ) << 8 ) | ( p_align & 0xFF ); }
    }  // namespace

    struct TLevelStore::TImpl
    {
        TPool<TSceneNode, TNodeTag> m_nodes;
    };

    TLevelStore::TLevelStore() : m_impl( std::make_unique<TImpl>() ) {}

    TLevelStore::~TLevelStore() { clear(); }

    TNodeHandle TLevelStore::createNode( std::string p_name )
    {
        TNodeHandle h = m_impl->m_nodes.create( std::move( p_name ) );
        if ( TSceneNode* n = m_impl->m_nodes.get( h ) )
        {
            n->m_handle = h;
            n->m_store  = this;
        }
        bumpTopology();
        return h;
    }

    void TLevelStore::destroyNode( TNodeHandle p_handle )
    {
        TSceneNode* root = getNode( p_handle );
        if ( root == nullptr ) return;

        if ( TSceneNode* parent = root->m_parent )
        {
            auto& kids = parent->m_children;
            kids.erase( std::remove( kids.begin(), kids.end(), root ), kids.end() );
            root->m_parent = nullptr;
        }

        // Iterative subtree walk.
        std::vector<TSceneNode*> order;
        std::vector<TSceneNode*> stack{ root };
        while ( !stack.empty() )
        {
            TSceneNode* n = stack.back();
            stack.pop_back();
            order.push_back( n );
            for ( TSceneNode* child : n->m_children )
                if ( child != nullptr ) stack.push_back( child );
        }

        for ( TSceneNode* n : order )
        {
            n->m_children.clear();
            n->m_parent = nullptr;

            std::vector<TComponent*> comps = std::move( n->m_components );
            n->m_components.clear();
            if ( n->m_ecs != nullptr )
            {
                for ( TComponent* c : comps ) n->m_ecs->destroyComponent( *n, *c );
                n->m_ecs = nullptr;
            }
            for ( TComponent* c : comps ) destroyComponent( c );

            const TNodeHandle h = n->m_handle;
            n->m_store          = nullptr;
            n->m_handle         = {};
            if ( h.valid() ) m_impl->m_nodes.destroy( h );
        }

        bumpTopology();
    }

    TSceneNode*       TLevelStore::getNode( TNodeHandle p_handle ) { return m_impl->m_nodes.get( p_handle ); }
    const TSceneNode* TLevelStore::getNode( TNodeHandle p_handle ) const { return m_impl->m_nodes.get( p_handle ); }

    TComponent* TLevelStore::adoptComponent( std::unique_ptr<TComponent> p_comp )
    {
        if ( !p_comp ) return nullptr;
        TComponent* raw = p_comp.release();
        m_components.emplace( raw, TComponentEntry{ nullptr, 0, 0, true } );
        return raw;
    }

    void TLevelStore::destroyComponent( TComponent* p_component )
    {
        if ( p_component == nullptr ) return;
        const auto it = m_components.find( p_component );
        if ( it == m_components.end() ) return;

        const TComponentEntry e = it->second;
        m_components.erase( it );

        if ( e.m_heap )
        {
            delete p_component;
            return;
        }
        if ( e.m_destroy ) e.m_destroy( p_component );
        recycleComponentSlot( p_component, e.m_size, e.m_align );
    }

    void TLevelStore::clear()
    {
        // Nodes first while component pointers are still valid.
        if ( m_impl )
        {
            m_impl->m_nodes.forEach(
                    [ & ]( TNodeHandle, TSceneNode& p_node )
                    {
                        if ( p_node.m_ecs != nullptr )
                        {
                            for ( TComponent* c : p_node.m_components ) p_node.m_ecs->destroyComponent( p_node, *c );
                            p_node.m_ecs = nullptr;
                        }
                        p_node.m_children.clear();
                        p_node.m_components.clear();
                        p_node.m_parent = nullptr;
                        p_node.m_store  = nullptr;
                        p_node.m_handle = {};
                    } );
            m_impl->m_nodes.clear();
        }

        for ( auto& [ ptr, e ] : m_components )
        {
            if ( ptr == nullptr ) continue;
            if ( e.m_heap )
                delete ptr;
            else if ( e.m_destroy )
                e.m_destroy( ptr );
        }
        m_components.clear();
        m_freeSlots.clear();
        m_componentArena.reset();
        bumpTopology();
    }

    size_t TLevelStore::nodeCount() const { return m_impl ? m_impl->m_nodes.liveCount() : 0; }

    void* TLevelStore::allocComponentSlot( size_t p_size, size_t p_align )
    {
        auto it = m_freeSlots.find( slotKey( p_size, p_align ) );
        if ( it != m_freeSlots.end() && !it->second.empty() )
        {
            void* mem = it->second.back();
            it->second.pop_back();
            return mem;
        }
        return m_componentArena.alloc( p_size, p_align );
    }

    void TLevelStore::recycleComponentSlot( void* p_mem, size_t p_size, size_t p_align )
    {
        if ( p_mem == nullptr ) return;
        m_freeSlots[ slotKey( p_size, p_align ) ].push_back( p_mem );
    }
}  // namespace Tomos
