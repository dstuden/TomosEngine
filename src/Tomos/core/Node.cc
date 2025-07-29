//
// Created by oskar on 1/19/25.
//

#include "Node.hh"
#include "Application.hh"

namespace Tomos
{
    Node::Node( const std::string& p_name ) :
        m_name( p_name )
    {
        m_layerId = Application::getState().config().get<std::string>( "unassignedLayerId" );
    }

    void Node::addChild( const std::shared_ptr<Node>& p_child )
    {
        // TODO: rework this so we don't have to climb the tree every time
        this->m_children.emplace( p_child );
        p_child->m_parent = this;
        p_child->m_active = this->m_active;
        p_child->traverse( []( Node& node )
                           {
                               node.m_layerId = node.m_parent->m_layerId;
                               node.m_active  = node.m_parent->m_active;
                           }
                           , []( Node& node )
                           {
                           } );
    }

    bool Node::removeChild( const std::shared_ptr<Node>& p_child )
    {
        return this->m_children.erase( p_child ) > 0;
    }

    void Node::traverse( const std::function<void( Node& )>& p_before, const std::function<void( Node& )>& p_after )
    {
        p_before( *this );
        for ( auto& current : m_children )
        {
            current->traverse( p_before, p_after );
        }
        p_after( *this );
    }

    void Node::setActive( bool p_active )
    {
        traverse(
                [p_active]( Node& node ) { node.m_active = p_active; },
                []( Node& )
                {
                } );
    }

    bool Node::isInScene() const
    {
        if ( dynamic_cast<const SceneNode*>( this ) != nullptr )
        {
            return true;
        }

        if ( m_parent == nullptr )
        {
            return false;
        }

        return m_parent->isInScene();
    }

    void Node::addComponent( const std::shared_ptr<Component>& p_component )
    {
        int res = Application::getState().ecs().registerComponent( p_component, shared_from_this() );
        if ( res != 0 )
        {
            LOG_WARN() << "Failed to register component";
            return;
        }


        this->m_components.push_back( p_component );
    }

    void Node::removeComponent( const std::shared_ptr<Component>& p_component )
    {
        int res = Application::getState().ecs().destroyComponent( p_component, shared_from_this() );
        if ( res != 0 )
        {
            LOG_WARN() << "Failed to destroy component";
            return;
        }

        auto it = std::find( m_components.begin(), m_components.end(), p_component );
        if ( it != m_components.end() )
        {
            m_components.erase( it );
        }
    }

    void Node::destroy()
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

    std::shared_ptr<Node> Node::findChild( const std::function<bool( Node& )>& predicate, unsigned int maxDepth ) const
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
    std::shared_ptr<T> Node::assertComponent() const
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
    std::shared_ptr<T> Node::assertChildComponent() const
    {
        for ( const auto& child : m_children )
        {
            auto component = child->assertComponent<T>();
            if ( component ) return component;
        }

        return nullptr;
    }

    std::shared_ptr<Node> Node::assertChildByName( const std::string& name, unsigned int maxDepth ) const
    {
        return findChild( [name]( Node& node ) { return node.m_name == name; }, maxDepth );
    }
} // namespace Tomos
