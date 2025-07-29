//
// Created by oskar on 1/19/25.
//

#pragma once

#include <functional>
#include <memory>
#include <set>

#include "Tomos/systems/Component.hh"
#include "Tomos/util/transform/Transform.hh"

namespace Tomos
{
    class Node : public std::enable_shared_from_this<Node>
    {
    public:
        explicit Node( const std::string& p_name = "UnnamedNode" );
        virtual ~Node() { destroy(); };

        void                                   addChild( const std::shared_ptr<Node>& p_child );
        bool                                   removeChild( const std::shared_ptr<Node>& p_child );
        void                                   traverse( const std::function<void( Node& )>& p_before, const std::function<void( Node& )>& p_after );
        const std::set<std::shared_ptr<Node>>& getChildren() const { return m_children; }

        void addComponent( const std::shared_ptr<Component>& p_component );
        void removeComponent( const std::shared_ptr<Component>& p_component );

        const std::vector<std::shared_ptr<Component>>& getComponents() const { return m_components; }

        bool isInScene() const;

        void destroy();

        // 333 those who know
        std::shared_ptr<Node> findChild( const std::function<bool( Node& )>& predicate, unsigned int maxDepth = 333 ) const;

        template<typename T>
        std::shared_ptr<T> assertComponent() const;

        template<typename T>
        std::shared_ptr<T> assertChildComponent() const;

        std::shared_ptr<Node> assertChildByName( const std::string& name, unsigned int maxDepth = 333 ) const;

        std::string m_name{};
        Transform   m_transform;

        void setActive( bool p_active );
        bool isActive() const { return m_active; }

        const std::string& getLayerId() const { return m_layerId; }

    protected:
        Node*                                   m_parent = nullptr;
        std::set<std::shared_ptr<Node>>         m_children;
        std::vector<std::shared_ptr<Component>> m_components;

        bool m_active = false;
        // start as unassigned
        std::string m_layerId;
    };

    static void updateTransforms( Node* node )
    {
        for ( const auto& c : node->getChildren() )
        {
            c->m_transform.updateGlobal( node->m_transform );
            updateTransforms( c.get() );
        }
    }

    class SceneNode : public Node
    {
    public:
        explicit SceneNode( const std::string& p_layerId, const std::string& p_name = "SceneNode" ) : Node( p_name ) { m_layerId = p_layerId; }

        void computeTransforms() { updateTransforms( this ); }
    };
}  // namespace Tomos
