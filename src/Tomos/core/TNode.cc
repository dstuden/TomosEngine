//
// Created by oskar on 1/19/25.
//

#include "TApplication.hh"
#include "TNode.hh"

namespace Tomos
{
    TNode::TNode( const std::string& p_name ) :
        m_name( p_name )
    {
        m_layerId = TApplication::getState().config().get<std::string>( "unassignedLayerId" );
    }

    void TNode::addChild( const std::shared_ptr<TNode>& p_child )
    {
        // TODO: rework this so we don't have to climb the tree every time
        this->m_children.emplace( p_child );
        p_child->m_parent = this;
        p_child->m_active = this->m_active;
        p_child->traverse( []( TNode& node )
                           {
                               node.m_layerId = node.m_parent->m_layerId;
                               node.m_active  = node.m_parent->m_active;
                           }
                           , []( TNode& node )
                           {
                           } );
    }

    bool TNode::removeChild( const std::shared_ptr<TNode>& p_child )
    {
        return this->m_children.erase( p_child ) > 0;
    }

    void TNode::traverse( const std::function<void( TNode& )>& p_before, const std::function<void( TNode& )>& p_after )
    {
        p_before( *this );
        for ( auto& current : m_children )
        {
            current->traverse( p_before, p_after );
        }
        p_after( *this );
    }

    void TNode::setActive( bool p_active )
    {
        traverse(
                [p_active]( TNode& node ) { node.m_active = p_active; },
                []( TNode& )
                {
                } );
    }

    bool TNode::isInScene() const
    {
        if ( dynamic_cast<const TSceneNode*>( this ) != nullptr )
        {
            return true;
        }

        if ( m_parent == nullptr )
        {
            return false;
        }

        return m_parent->isInScene();
    }

    void TNode::addComponent( const std::shared_ptr<TComponent>& p_component )
    {
        int res = TApplication::getState().ecs().registerComponent( p_component, shared_from_this() );
        if ( res != 0 )
        {
            TLOG_WARN() << "Failed to register component";
            return;
        }


        this->m_components.push_back( p_component );
    }

    void TNode::removeComponent( const std::shared_ptr<TComponent>& p_component )
    {
        int res = TApplication::getState().ecs().destroyComponent( p_component, shared_from_this() );
        if ( res != 0 )
        {
            TLOG_WARN() << "Failed to destroy component";
            return;
        }

        auto it = std::find( m_components.begin(), m_components.end(), p_component );
        if ( it != m_components.end() )
        {
            m_components.erase( it );
        }
    }

    void TNode::destroy()
    {
        for ( const auto& component : m_components )
        {
            removeComponent( component );
        }

        for ( const auto& child : m_children )
        {
            child->destroy();
        }

        if ( m_parent )
        {
            m_parent->removeChild( shared_from_this() );
        }
    }

    std::shared_ptr<TNode> TNode::findChild( const std::function<bool( TNode& )>& predicate, unsigned int maxDepth ) const
    {
        if ( maxDepth == 0 ) return nullptr;

        for ( const auto& child : m_children )
        {
            if ( predicate( *child ) )
            {
                return child;
            }

            auto found = child->findChild( predicate, maxDepth - 1 );
            if ( found ) return found;
        }

        return nullptr;
    }


    template<typename T>
    std::shared_ptr<T> TNode::assertComponent() const
    {
        for ( const auto& component : m_components )
        {
            if ( dynamic_cast<T*>( component.get() ) != nullptr )
            {
                return component;
            }
        }

        return nullptr;
    }

    template<typename T>
    std::shared_ptr<T> TNode::assertChildComponent() const
    {
        for ( const auto& child : m_children )
        {
            auto component = child->assertComponent<T>();
            if ( component ) return component;
        }

        return nullptr;
    }

    std::shared_ptr<TNode> TNode::assertChildByName( const std::string& name, unsigned int maxDepth ) const
    {
        return findChild( [name]( TNode& node ) { return node.m_name == name; }, maxDepth );
    }
} // namespace Tomos
