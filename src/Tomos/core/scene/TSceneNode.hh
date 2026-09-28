#pragma once

#include <algorithm>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "Tomos/core/ecs/TECS.hh"
#include "Tomos/systems/TComponent.hh"
#include "Tomos/util/memory/THandle.hh"
#include "Tomos/util/memory/TLevelStore.hh"
#include "Tomos/util/transform/TTransform.hh"

namespace Tomos
{
    // Pooled graph node, or the unpooled TScene root. Children/components are store-owned.
    class TSceneNode
    {
    public:
        uint64_t    m_id   = 0;
        std::string m_name = "<unnamed>";
        TTransform  m_transform;
        bool        m_dynamic = true;

        TSceneNode();
        explicit TSceneNode( std::string p_name );
        virtual ~TSceneNode();

        TSceneNode( const TSceneNode& )            = delete;
        TSceneNode& operator=( const TSceneNode& ) = delete;

        void addChild( TSceneNode* p_child );

        // Detach only — does not destroy. Use store()->destroyNode(handle()) to free.
        void removeChild( TSceneNode* p_child );

        void clearChildren();

        // No-op if p_newParent is null, self, or a descendant.
        void reparent( TSceneNode* p_newParent );

        [[nodiscard]] TSceneNode*                     getParent() const { return m_parent; }
        [[nodiscard]] const std::vector<TSceneNode*>& getChildren() const { return m_children; }
        [[nodiscard]] TNodeHandle                     handle() const { return m_handle; }
        [[nodiscard]] TLevelStore*                    store() const { return m_store; }

        // Prefer for physics/editor; getGlobalMatrix() only after computeTransforms.
        [[nodiscard]] glm::mat4 worldMatrix() const;

        TSceneNode*       findById( uint64_t p_id );
        const TSceneNode* findById( uint64_t p_id ) const;

        TSceneNode*       findByName( const std::string& p_name );
        const TSceneNode* findByName( const std::string& p_name ) const;

        void setId( uint64_t p_id );

        static uint64_t allocateId();
        static void     noteAllocatedId( uint64_t p_id );

        template<typename T, typename... Args>
        T& emplaceComponent( Args&&... p_args )
        {
            static_assert( std::is_base_of_v<TComponent, T>, "T must derive from TComponent" );
            if ( m_store == nullptr ) throw std::runtime_error( "emplaceComponent requires a level store (use TScene::createNode)" );
            T* comp = m_store->createComponent<T>( std::forward<Args>( p_args )... );
            if ( m_ecs != nullptr ) m_ecs->registerComponent( *this, *comp );
            m_components.push_back( comp );
            return *comp;
        }

        void addComponent( std::unique_ptr<TComponent> p_component );

        template<typename T>
        void addComponent( std::unique_ptr<T> p_component )
        {
            static_assert( std::is_base_of_v<TComponent, T>, "T must derive from TComponent" );
            addComponent( std::unique_ptr<TComponent>( std::move( p_component ) ) );
        }

        template<typename T>
        void removeComponent( T* p_component )
        {
            static_assert( std::is_base_of_v<TComponent, T>, "T must derive from TComponent" );
            if ( m_ecs != nullptr ) m_ecs->destroyComponent( *this, *p_component );
            m_components.erase( std::remove( m_components.begin(), m_components.end(), static_cast<TComponent*>( p_component ) ),
                                m_components.end() );
            if ( m_store != nullptr ) m_store->destroyComponent( p_component );
        }

        template<typename T>
        T& getComponent()
        {
            for ( TComponent* c : m_components )
            {
                if ( auto* ptr = dynamic_cast<T*>( c ) ) return *ptr;
            }
            throw std::runtime_error( "Component not found on node: " + m_name );
        }

        template<typename T>
        T* findComponent()
        {
            for ( TComponent* c : m_components )
            {
                if ( auto* ptr = dynamic_cast<T*>( c ) ) return ptr;
            }
            return nullptr;
        }

        [[nodiscard]] const std::vector<TComponent*>& getComponents() const { return m_components; }

        std::string tree() const;

    protected:
        void attachComponents( TECS& p_ecs );
        void detachComponents();

        TLevelStore* m_store = nullptr;

    private:
        friend class TLevelStore;
        friend class TScene;

        bool isDescendantOf( const TSceneNode* p_ancestor ) const;

        TECS*                    m_ecs    = nullptr;
        TSceneNode*              m_parent = nullptr;
        TNodeHandle              m_handle{};
        std::vector<TSceneNode*> m_children;
        std::vector<TComponent*> m_components;
    };
}  // namespace Tomos
