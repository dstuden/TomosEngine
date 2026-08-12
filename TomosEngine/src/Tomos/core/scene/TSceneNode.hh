#pragma once

#include <algorithm>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

#include "Tomos/core/ecs/TECS.hh"
#include "Tomos/systems/TComponent.hh"
#include "Tomos/util/transform/TTransform.hh"

namespace Tomos
{
    class TSceneNode : public std::enable_shared_from_this<TSceneNode>
    {
    public:
        uint64_t    m_id   = 0;
        std::string m_name = "<unnamed>";
        TTransform  m_transform;
        bool        m_dynamic = true;

        TSceneNode();
        explicit TSceneNode( std::string p_name );
        virtual ~TSceneNode();

        void addChild( std::shared_ptr<TSceneNode> p_child );
        void removeChild( const TSceneNode* p_child );
        void clearChildren();

        // No-op if p_newParent is null, self, or a descendant.
        void reparent( const std::shared_ptr<TSceneNode>& p_newParent );

        std::shared_ptr<TSceneNode>                     getParent() const { return m_parent.lock(); }
        const std::vector<std::shared_ptr<TSceneNode>>& getChildren() const { return m_children; }

        // Prefer for physics/editor; getGlobalMatrix() only after computeTransforms.
        [[nodiscard]] glm::mat4 worldMatrix() const;

        TSceneNode*                 findById( uint64_t p_id );
        const TSceneNode*           findById( uint64_t p_id ) const;
        std::shared_ptr<TSceneNode> findSharedById( uint64_t p_id );

        TSceneNode*       findByName( const std::string& p_name );
        const TSceneNode* findByName( const std::string& p_name ) const;

        void setId( uint64_t p_id );

        static uint64_t allocateId();
        static void     noteAllocatedId( uint64_t p_id );

        template<typename T>
        void addComponent( std::shared_ptr<T> p_component )
        {
            static_assert( std::is_base_of_v<TComponent, T>, "T must derive from TComponent" );
            if ( m_ecs != nullptr ) m_ecs->registerComponent( *this, *p_component );
            m_components.push_back( std::move( p_component ) );
        }

        template<typename T>
        void removeComponent( T* p_component )
        {
            static_assert( std::is_base_of_v<TComponent, T>, "T must derive from TComponent" );
            if ( m_ecs != nullptr ) m_ecs->destroyComponent( *this, *p_component );
            m_components.erase( std::remove_if( m_components.begin(), m_components.end(), [ p_component ]( const auto& c ) { return c.get() == p_component; } ),
                                m_components.end() );
        }

        template<typename T>
        T& getComponent()
        {
            for ( auto& c : m_components )
            {
                if ( auto* ptr = dynamic_cast<T*>( c.get() ) ) return *ptr;
            }
            throw std::runtime_error( "Component not found on node: " + m_name );
        }

        template<typename T>
        T* findComponent()
        {
            for ( auto& c : m_components )
            {
                if ( auto* ptr = dynamic_cast<T*>( c.get() ) ) return ptr;
            }
            return nullptr;
        }

        const std::vector<std::shared_ptr<TComponent>>& getComponents() const { return m_components; }

        std::string tree() const;

    protected:
        void attachComponents( TECS& p_ecs );
        void detachComponents();

    private:
        bool isDescendantOf( const TSceneNode* p_ancestor ) const;

        TECS*                     m_ecs = nullptr;
        std::weak_ptr<TSceneNode> m_parent;

        std::vector<std::shared_ptr<TSceneNode>> m_children;
        std::vector<std::shared_ptr<TComponent>> m_components;
    };
}  // namespace Tomos
