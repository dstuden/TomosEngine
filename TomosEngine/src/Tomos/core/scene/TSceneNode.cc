#include "Tomos/core/scene/TSceneNode.hh"

#include <atomic>
#include <ranges>
#include <utility>

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
        // Leaf → root: world = … * parentLocal * local (no heap).
        glm::mat4 world = m_transform.getLocalMatrix();
        for ( auto parent = getParent(); parent; parent = parent->getParent() ) world = parent->m_transform.getLocalMatrix() * world;
        return world;
    }

    void TSceneNode::addChild( std::shared_ptr<TSceneNode> p_child )
    {
        p_child->m_parent = this->weak_from_this();
        if ( m_ecs != nullptr ) p_child->attachComponents( *m_ecs );
        m_children.push_back( std::move( p_child ) );
    }

    void TSceneNode::removeChild( const TSceneNode* p_child )
    {
        auto it = std::find_if( m_children.begin(), m_children.end(), [ p_child ]( const auto& p_c ) { return p_c.get() == p_child; } );
        if ( it != m_children.end() )
        {
            ( *it )->detachComponents();
            ( *it )->m_parent.reset();
            m_children.erase( it );
        }
    }

    void TSceneNode::clearChildren()
    {
        while ( !m_children.empty() ) removeChild( m_children.back().get() );
    }

    bool TSceneNode::isDescendantOf( const TSceneNode* p_ancestor ) const
    {
        for ( const TSceneNode* n = this; n != nullptr; )
        {
            if ( n == p_ancestor ) return true;
            auto parent = n->m_parent.lock();
            n           = parent.get();
        }
        return false;
    }

    void TSceneNode::reparent( const std::shared_ptr<TSceneNode>& p_newParent )
    {
        if ( p_newParent == nullptr || p_newParent.get() == this ) return;
        if ( p_newParent->isDescendantOf( this ) ) return;  // would create a cycle

        auto self = shared_from_this();
        if ( auto old = getParent() ) old->removeChild( this );
        p_newParent->addChild( std::move( self ) );
    }

    TSceneNode* TSceneNode::findById( uint64_t p_id )
    {
        std::vector<TSceneNode*> stack = { this };
        while ( !stack.empty() )
        {
            TSceneNode* node = stack.back();
            stack.pop_back();
            if ( node->m_id == p_id ) return node;
            for ( auto& child : node->m_children ) stack.push_back( child.get() );
        }
        return nullptr;
    }

    const TSceneNode* TSceneNode::findById( uint64_t p_id ) const { return const_cast<TSceneNode*>( this )->findById( p_id ); }

    std::shared_ptr<TSceneNode> TSceneNode::findSharedById( uint64_t p_id )
    {
        if ( m_id == p_id )
        {
            try
            {
                return shared_from_this();
            }
            catch ( const std::bad_weak_ptr& )
            {
                return nullptr;  // not owned by shared_ptr (e.g. TScene by value)
            }
        }

        std::vector<std::shared_ptr<TSceneNode>> stack( m_children.begin(), m_children.end() );
        while ( !stack.empty() )
        {
            auto node = stack.back();
            stack.pop_back();
            if ( node->m_id == p_id ) return node;
            for ( auto& child : node->m_children ) stack.push_back( child );
        }
        return nullptr;
    }

    TSceneNode* TSceneNode::findByName( const std::string& p_name )
    {
        std::vector<TSceneNode*> stack = { this };
        while ( !stack.empty() )
        {
            TSceneNode* node = stack.back();
            stack.pop_back();
            if ( node->m_name == p_name ) return node;
            for ( auto& child : node->m_children ) stack.push_back( child.get() );
        }
        return nullptr;
    }

    const TSceneNode* TSceneNode::findByName( const std::string& p_name ) const { return const_cast<TSceneNode*>( this )->findByName( p_name ); }

    // Iterative DFS to avoid recursion (satisfies misc-no-recursion)
    void TSceneNode::attachComponents( TECS& p_ecs )
    {
        std::vector<TSceneNode*> stack = { this };
        while ( !stack.empty() )
        {
            TSceneNode* node = stack.back();
            stack.pop_back();
            node->m_ecs = &p_ecs;
            for ( auto& c : node->m_components ) p_ecs.registerComponent( *node, *c );
            for ( auto& child : node->m_children ) stack.push_back( child.get() );
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
                for ( auto& c : node->m_components ) node->m_ecs->destroyComponent( *node, *c );
                node->m_ecs = nullptr;
            }
            for ( auto& child : node->m_children ) stack.push_back( child.get() );
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
            for ( const auto& it : std::views::reverse( node->m_children ) ) stack.emplace_back( it.get(), depth + 1 );
        }
        return result;
    }
}  // namespace Tomos
