//
// Created by oskar on 1/19/25.
//

#pragma once

#include <functional>
#include <memory>
#include <set>
#include <vector>

#include "Tomos/systems/TComponent.hh"
#include "Tomos/util/conf/TConfig.hh"
#include "Tomos/util/transform/TTransform.hh"

namespace Tomos
{
    /**
     * Nodes must be created with shared_ptr to allow for shared ownership.
     * Otherwise, the shared_from_this() method will fail - unlucky; L
     */
    class TNode : public std::enable_shared_from_this<TNode>
    {
        friend class TSceneNode;

    public:
        static std::shared_ptr<TNode> create( const std::string& p_name = "UnnamedNode" );

        virtual ~TNode() = default;

        void                                    addChild( const std::shared_ptr<TNode>& p_child );
        bool                                    removeChild( const std::shared_ptr<TNode>& p_child );
        void                                    traverse( const std::function<void( TNode& )>& p_before, const std::function<void( TNode& )>& p_after );
        const std::set<std::shared_ptr<TNode>>& getChildren() const { return m_children; }

        void addComponent( const std::shared_ptr<TComponent>& p_component );
        void removeComponent( const std::shared_ptr<TComponent>& p_component );

        const std::vector<std::shared_ptr<TComponent>>& getComponents() const { return m_components; }

        virtual bool isInScene() const;

        void destroy();

        std::shared_ptr<TNode> findChild( const std::function<bool( TNode& )>& p_predicate,
                                          unsigned int                         p_maxDepth = Global::config.get<int>( "maxNodeDepth" ) ) const;

        template<typename T>
        std::shared_ptr<T> findComponent() const;

        template<typename T>
        std::shared_ptr<T> findChildComponent() const;

        std::shared_ptr<TNode> findChildByName( const std::string& p_name, unsigned int p_maxDepth = Global::config.get<int>( "maxNodeDepth" ) ) const;

        const std::string& getName() const { return m_name; }
        const TTransform&  getTransform() const { return m_transform; }
        TTransform&        getTransform() { return m_transform; }  // Non-const version for modification

        void setActive( bool p_active );
        bool isActive() const { return m_active; }

        const std::string& getLayerId() const { return m_layerId; }

        std::shared_ptr<TNode> getParent() const { return m_parent.lock(); }

    protected:
        // Use std::weak_ptr to prevent a circular reference with children.
        std::weak_ptr<TNode>                     m_parent;
        std::set<std::shared_ptr<TNode>>         m_children;
        std::vector<std::shared_ptr<TComponent>> m_components;

        std::string m_name{};
        TTransform  m_transform;
        bool        m_active = false;
        std::string m_layerId;

    private:
        // Private constructor to enforce use of create() method
        explicit TNode( const std::string& p_name = "UnnamedNode" );
    };

    static void updateTransforms( TNode* p_TNode )
    {
        for ( const auto& c : p_TNode->getChildren() )
        {
            c->getTransform().updateGlobal( p_TNode->getTransform() );
            updateTransforms( c.get() );
        }
    }

    class TSceneNode : public TNode
    {
    public:
        static std::shared_ptr<TSceneNode> create( const std::string& p_layerId, const std::string& p_name = "SceneNode" );

        // Override isInScene for better polymorphism
        bool isInScene() const override { return true; }

        void computeTransforms() { updateTransforms( this ); }

    private:
        explicit TSceneNode( const std::string& p_layerId, const std::string& p_name = "SceneNode" ) : TNode( p_name ) { m_layerId = p_layerId; }
    };
}  // namespace Tomos
