//
// Created by dstuden on 4/1/25.
//

#include "TMeshSystem.hh"
#include "Tomos/core/TApplication.hh"
#include "Tomos/systems/camera/TCameraSystem.hh"

namespace Tomos
{
    void TMeshSystem::componentAdded( const std::shared_ptr<TComponent>& p_component, const std::shared_ptr<TNode>& p_node )
    {
        auto meshComponent = std::dynamic_pointer_cast<TMeshComponent>( p_component );
        if ( m_components.contains( p_node->getLayerId() ) && !m_components[p_node->getLayerId()].erase( p_component ) )
        {
            TLOG_DEBUG() << "Component moved to new node: " << p_component->m_name;
        }
        m_components[p_node->getLayerId()][p_component] = p_node;
    }

    void TMeshSystem::componentRemoved( const std::shared_ptr<TComponent>& p_component, const std::shared_ptr<TNode>& p_node )
    {
        if ( m_components.contains( p_node->getLayerId() ) ) m_components[p_node->getLayerId()].erase( p_component );
    }

    void TMeshSystem::lateUpdate( const std::string& p_layerId )
    {
        // Get view projection matrix once
        auto sys      = TApplication::getState().ecs().getSystem<TCameraSystem>();

        if ( !m_components.contains( p_layerId ) )
        {
            return;
        }

        m_drawCalls[p_layerId].clear();

        for ( auto& [component, node] : m_components[p_layerId] )
        {
            if ( !node->isActive() ) continue;

            auto meshComponent = std::dynamic_pointer_cast<TMeshComponent>( component );
            auto mesh          = meshComponent->getMesh();
            auto material      = meshComponent->getMaterial();

            if ( mesh && material )
            {
                m_drawCalls[p_layerId].push_back( { mesh->getShader(), material, mesh->getVertexArray(), node->getTransform().m_globMat } );
            }
        }
    }

    const std::vector<DrawCall>& TMeshSystem::getDrawCalls( const std::string& layerId ) const
    {
        static const std::vector<DrawCall> empty;
        auto                               it = m_drawCalls.find( layerId );
        return it != m_drawCalls.end() ? it->second : empty;
    }

    void TMeshSystem::clearDrawCalls( const std::string& layerId ) { m_drawCalls[layerId].clear(); }
}  // namespace Tomos
