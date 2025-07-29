//
// Created by dstuden on 4/1/25.
//

#include "MeshSystem.hh"

#include "Tomos/core/Application.hh"
#include "Tomos/systems/camera/CameraSystem.hh"

namespace Tomos
{
    void MeshSystem::componentAdded( const std::shared_ptr<Component>& p_component, const std::shared_ptr<Node>& p_node )
    {
        auto meshComponent = std::dynamic_pointer_cast<MeshComponent>( p_component );
        if ( m_components.contains( p_node->getLayerId() ) && !m_components[p_node->getLayerId()].erase( p_component ) )
        {
            LOG_DEBUG() << "Component moved to new node: " << p_component->m_name;
        }
        m_components[p_node->getLayerId()][p_component] = p_node;
    }

    void MeshSystem::componentRemoved( const std::shared_ptr<Component>& p_component, const std::shared_ptr<Node>& p_node )
    {
        if ( m_components.contains( p_node->getLayerId() ) ) m_components[p_node->getLayerId()].erase( p_component );
    }

    void MeshSystem::lateUpdate( const std::string& p_layerId )
    {
        // Get view projection matrix once
        auto sys      = Application::getState().ecs().getSystem<CameraSystem>();

        if ( !m_components.contains( p_layerId ) )
        {
            return;
        }

        m_drawCalls[p_layerId].clear();

        for ( auto& [component, node] : m_components[p_layerId] )
        {
            if ( !node->isActive() ) continue;

            auto meshComponent = std::dynamic_pointer_cast<MeshComponent>( component );
            auto mesh          = meshComponent->getMesh();
            auto material      = meshComponent->getMaterial();

            if ( mesh && material )
            {
                m_drawCalls[p_layerId].push_back( { mesh->getShader(), material, mesh->getVertexArray(), node->m_transform.m_globMat } );
            }
        }
    }

    const std::vector<DrawCall>& MeshSystem::getDrawCalls( const std::string& layerId ) const
    {
        static const std::vector<DrawCall> empty;
        auto                               it = m_drawCalls.find( layerId );
        return it != m_drawCalls.end() ? it->second : empty;
    }

    void MeshSystem::clearDrawCalls( const std::string& layerId ) { m_drawCalls[layerId].clear(); }
}  // namespace Tomos
