//
// Created by oskar on 1/19/25.
//

#include "TNode.hh"

#include "TApplication.hh"
#include "Tomos/util/conf/TConfig.hh"

namespace Tomos
{
    std::shared_ptr<TNode> TNode::create( const std::string& p_name )
    {
        // Yes I know, this is a stupid but the constructor is private and we need to create an instance
        // managed by a shared_ptr from the very beginning.
        // This is what makes shared_from_this() work.
        return std::shared_ptr<TNode>( new TNode( p_name ) );
    }

    TNode::TNode( const std::string& p_name ) : m_name( p_name ) { m_layerId = Global::config.get<std::string>( "unassignedLayerId" ); }

    void TNode::addChild( const std::shared_ptr<TNode>& p_child )
    {
        // Check for self-parenting
        if ( p_child.get() == this )
        {
            TLOG_ERROR() << "Trying to self-parent: " << p_child->getName();
            throw std::runtime_error( "Failed to add child to node: " + m_name );
        }

        if ( auto oldParent = p_child->m_parent.lock() )
        {
            oldParent->removeChild( p_child );
        }

        this->m_children.emplace( p_child );
        p_child->m_parent = shared_from_this();

        p_child->m_layerId = this->m_layerId;
        p_child->m_active  = this->m_active;
        // Traverse to update the rest of the subtree
        p_child->traverse(
                [this]( TNode& p_node )
                {
                    p_node.m_layerId = this->m_layerId;
                    p_node.m_active  = this->m_active;
                },
                []( TNode& ) {} );
    }

    bool TNode::removeChild( const std::shared_ptr<TNode>& p_child )
    {
        if ( this->m_children.erase( p_child ) > 0 )
        {
            // Nullify the parent pointer in the removed child to prevent dangling pointers.
            p_child->m_parent.reset();
            return true;
        }
        return false;
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
        traverse( [p_active]( TNode& node ) { node.m_active = p_active; }, []( TNode& ) {} );
    }

    bool TNode::isInScene() const
    {
        if ( auto parentPtr = m_parent.lock() )
        {
            return parentPtr->isInScene();
        }
        return false;
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
        std::vector<std::shared_ptr<TComponent>> componentsToDestroy = m_components;
        for ( const auto& component : componentsToDestroy )
        {
            removeComponent( component );
        }

        std::set<std::shared_ptr<TNode>> childrenToDestroy = m_children;
        for ( const auto& child : childrenToDestroy )
        {
            child->destroy();
        }

        if ( auto parentPtr = m_parent.lock() )
        {
            parentPtr->removeChild( shared_from_this() );
        }
    }

    std::shared_ptr<TNode> TNode::findChild( const std::function<bool( TNode& )>& p_predicate, unsigned int p_maxDepth ) const
    {
        if ( p_maxDepth == 0 ) return nullptr;

        for ( const auto& child : m_children )
        {
            if ( p_predicate( *child ) )
            {
                return child;
            }
            auto found = child->findChild( p_predicate, p_maxDepth - 1 );
            if ( found )
            {
                return found;
            }
        }
        return nullptr;
    }

    std::shared_ptr<TNode> TNode::findChildByName( const std::string& p_name, unsigned int p_maxDepth ) const
    {
        return findChild( [p_name]( TNode& node ) { return node.getName() == p_name; }, p_maxDepth );
    }

    template<typename T>
    std::shared_ptr<T> TNode::findComponent() const
    {
        for ( const auto& component : m_components )
        {
            if ( auto castedComponent = std::dynamic_pointer_cast<T>( component ) )
            {
                return castedComponent;
            }
        }
        return nullptr;
    }

    template<typename T>
    std::shared_ptr<T> TNode::findChildComponent() const
    {
        for ( const auto& child : m_children )
        {
            if ( auto component = child->findComponent<T>() )
            {
                return component;
            }
        }
        return nullptr;
    }

    std::shared_ptr<TSceneNode> TSceneNode::create( const std::string& p_layerId, const std::string& p_name )
    {
        // Use std::make_shared to create a TSceneNode instance managed by a shared_ptr.
        return std::shared_ptr<TSceneNode>( new TSceneNode( p_layerId, p_name ) );
    }
}  // namespace Tomos
