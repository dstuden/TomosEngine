#include "Tomos/core/scene/TSceneNode.hh"

#include <atomic>
#include <ranges>
#include <utility>

#include "Tomos/util/memory/TArenaAllocator.hh"
#include "Tomos/util/memory/TFrameAllocator.hh"

namespace Tomos
{
    namespace
    {
        std::atomic<uint64_t> g_nextNodeId{ 1 };
    }

    uint64_t TSceneNode::allocateId() { return g_nextNodeId.fetch_add( 1 ); }

    void TSceneNode::noteAllocatedId( uint64_t p_id )
    {
        uint64_t cur = g_nextNodeId.load();
        while ( p_id >= cur && !g_nextNodeId.compare_exchange_weak( cur, p_id + 1 ) )
        {
        }
    }

    TSceneNode::TSceneNode() : m_id( allocateId() ) {}

    TSceneNode::TSceneNode( std::string p_name ) : m_id( allocateId() ), m_name( std::move( p_name ) ) {}

    TSceneNode::~TSceneNode()
    {
        if ( m_ecs != nullptr ) detachComponents();
    }

    void TSceneNode::setId( uint64_t p_id )
    {
        m_id = p_id;
        noteAllocatedId( p_id );
    }

    glm::mat4 TSceneNode::worldMatrix() const
    {
        glm::mat4 world = m_transform.getLocalMatrix();
        for ( TSceneNode* parent = m_parent; parent != nullptr; parent = parent->m_parent )
            world = parent->m_transform.getLocalMatrix() * world;
        return world;
    }

    void TSceneNode::addChild( TSceneNode* p_child )
    {
        if ( p_child == nullptr || p_child == this ) return;
        if ( p_child->m_parent == this ) return;

        if ( p_child->m_parent != nullptr ) p_child->m_parent->removeChild( p_child );

        if ( p_child->m_store == nullptr && m_store != nullptr ) p_child->m_store = m_store;

        p_child->m_parent = this;
        if ( m_ecs != nullptr ) p_child->attachComponents( *m_ecs );
        m_children.push_back( p_child );
        if ( m_store != nullptr ) m_store->bumpTopology();
    }

    void TSceneNode::removeChild( TSceneNode* p_child )
    {
        auto it = std::find( m_children.begin(), m_children.end(), p_child );
        if ( it == m_children.end() ) return;
        ( *it )->detachComponents();
        ( *it )->m_parent = nullptr;
        m_children.erase( it );
        if ( m_store != nullptr ) m_store->bumpTopology();
    }

    void TSceneNode::clearChildren()
    {
        if ( m_store != nullptr )
        {
            std::vector<TSceneNode*> kids = m_children;
            m_children.clear();
            for ( TSceneNode* child : kids )
            {
                if ( child != nullptr && child->m_handle.valid() )
                    m_store->destroyNode( child->m_handle );
                else if ( child != nullptr )
                {
                    child->m_parent = nullptr;
                    child->detachComponents();
                }
            }
            return;
        }

        while ( !m_children.empty() ) removeChild( m_children.back() );
    }

    bool TSceneNode::isDescendantOf( const TSceneNode* p_ancestor ) const
    {
        for ( const TSceneNode* n = this; n != nullptr; n = n->m_parent )
        {
            if ( n == p_ancestor ) return true;
        }
        return false;
    }

    void TSceneNode::reparent( TSceneNode* p_newParent )
    {
        if ( p_newParent == nullptr || p_newParent == this ) return;
        if ( p_newParent->isDescendantOf( this ) ) return;

        if ( m_parent != nullptr ) m_parent->removeChild( this );
        p_newParent->addChild( this );
    }

    namespace
    {
        // Iterative DFS; frame-arena stack when available, else heap.
        template<typename Pred>
        TSceneNode* findIf( TSceneNode* p_root, Pred&& p_pred )
        {
            auto search = [ & ]( auto& stack ) -> TSceneNode*
            {
                stack.push_back( p_root );
                while ( !stack.empty() )
                {
                    TSceneNode* node = stack.back();
                    stack.pop_back();
                    if ( p_pred( *node ) ) return node;
                    for ( TSceneNode* child : node->getChildren() ) stack.push_back( child );
                }
                return nullptr;
            };

            if ( TFrameAllocator* fa = TFrameAllocator::current() )
            {
                TArenaVector<TSceneNode*> stack( TArenaAllocator<TSceneNode*>( fa->arena() ) );
                return search( stack );
            }
            std::vector<TSceneNode*> stack;
            return search( stack );
        }
    }  // namespace

    TSceneNode* TSceneNode::findById( uint64_t p_id )
    {
        return findIf( this, [ p_id ]( const TSceneNode& p_n ) { return p_n.m_id == p_id; } );
    }

    const TSceneNode* TSceneNode::findById( uint64_t p_id ) const { return const_cast<TSceneNode*>( this )->findById( p_id ); }

    TSceneNode* TSceneNode::findByName( const std::string& p_name )
    {
        return findIf( this, [ &p_name ]( const TSceneNode& p_n ) { return p_n.m_name == p_name; } );
    }

    const TSceneNode* TSceneNode::findByName( const std::string& p_name ) const { return const_cast<TSceneNode*>( this )->findByName( p_name ); }

    void TSceneNode::addComponent( std::unique_ptr<TComponent> p_component )
    {
        if ( !p_component ) return;
        if ( m_store == nullptr ) throw std::runtime_error( "addComponent requires a level store (use TScene::createNode)" );
        TComponent* raw = m_store->adoptComponent( std::move( p_component ) );
        if ( m_ecs != nullptr ) m_ecs->registerComponent( *this, *raw );
        m_components.push_back( raw );
    }

    void TSceneNode::attachComponents( TECS& p_ecs )
    {
        std::vector<TSceneNode*> stack = { this };
        while ( !stack.empty() )
        {
            TSceneNode* node = stack.back();
            stack.pop_back();
            node->m_ecs = &p_ecs;
            for ( TComponent* c : node->m_components ) p_ecs.registerComponent( *node, *c );
            for ( TSceneNode* child : node->m_children ) stack.push_back( child );
        }
    }

    void TSceneNode::detachComponents()
    {
        std::vector<TSceneNode*> stack = { this };
        while ( !stack.empty() )
        {
            TSceneNode* node = stack.back();
            stack.pop_back();
            if ( node->m_ecs != nullptr )
            {
                for ( TComponent* c : node->m_components ) node->m_ecs->destroyComponent( *node, *c );
                node->m_ecs = nullptr;
            }
            for ( TSceneNode* child : node->m_children ) stack.push_back( child );
        }
    }

    std::string TSceneNode::tree() const
    {
        std::string                                    result;
        std::vector<std::pair<const TSceneNode*, int>> stack = { { this, 0 } };
        while ( !stack.empty() )
        {
            auto [ node, depth ] = stack.back();
            stack.pop_back();
            result += std::string( static_cast<size_t>( depth * 2 ), ' ' ) + node->m_name + "\n";
            for ( const auto& it : std::views::reverse( node->m_children ) ) stack.emplace_back( it, depth + 1 );
        }
        return result;
    }
}  // namespace Tomos
