//
// Created by dstuden on 1/20/25.
//

#include "CameraSystem.hh"

#include "Tomos/util/logger/Logger.hh"

namespace Tomos
{
    void CameraSystem::componentAdded( const std::shared_ptr<Component>& p_component, const std::shared_ptr<Node>& p_node )
    {
        if ( m_components.contains( p_node->getLayerId() ) && !m_components[p_node->getLayerId()].erase( p_component ) )
        {
            LOG_DEBUG() << "Component moved to new node: " << p_component->m_name;
        }
        m_components[p_node->getLayerId()][p_component] = p_node;
    }

    void CameraSystem::componentRemoved( const std::shared_ptr<Component>& p_component, const std::shared_ptr<Node>& p_node )
    {
        if ( m_components.contains( p_node->getLayerId() ) )
            m_components[p_node->getLayerId()].erase( p_component );
    }

    void CameraSystem::update( const std::string& p_layerId )
    {
        if ( !m_components.contains( p_layerId ) )
        {
            m_layerActiveData.erase( p_layerId );
            return;
        }

        auto cam = std::find_if( m_components[p_layerId].begin(), m_components[p_layerId].end(),
                                 []( std::pair<std::shared_ptr<Component>, std::shared_ptr<Node>> p_pair )
                                 {
                                     auto c = std::dynamic_pointer_cast<CameraComponent>( p_pair.first );
                                     return ( c && c->m_active && p_pair.second->isActive() );
                                 } );

        if ( cam == m_components[p_layerId].end() )
        {
            LOG_WARN() << "No active camera found";
            m_layerActiveData.erase( p_layerId );
            return;
        }

        const auto cc = std::dynamic_pointer_cast<CameraComponent>( cam->first );
        const auto tc = cam->second->m_transform;

        m_layerActiveData[p_layerId].m_activeCamera   = cc;
        m_layerActiveData[p_layerId].m_viewMat        = tc.m_globInvMat;
        m_layerActiveData[p_layerId].m_viewMatInv     = tc.m_globMat;
        m_layerActiveData[p_layerId].m_viewProjMat    = cc->getProjection() * m_layerActiveData[p_layerId].m_viewMat;
        m_layerActiveData[p_layerId].m_viewProjMatInv = m_layerActiveData[p_layerId].m_viewMatInv * cc->getInvProjection();
    }

    std::shared_ptr<Node> CameraSystem::getActiveCameraNode( const std::string& p_layerId )
    {
        if ( !m_components.contains( p_layerId ) )
        {
            LOG_DEBUG() << "No active camera in layer: " << p_layerId;
            m_layerActiveData.erase( p_layerId );

            return nullptr;
        }

        auto c = std::dynamic_pointer_cast<Component>( m_layerActiveData[p_layerId].m_activeCamera );
        if ( c )
        {
            auto n = m_components[p_layerId].find( c );
            if ( n != m_components[p_layerId].end() )
            {
                return n->second;
            }
        }

        return nullptr;
    }

    const std::shared_ptr<CameraComponent>& CameraSystem::getActiveCamera( const std::string& p_layerId )
    {
        if ( !m_layerActiveData.contains( p_layerId ) )
        {
            return m_defaultCameraData.m_activeCamera;
        }

        return m_layerActiveData[p_layerId].m_activeCamera;
    }

    const glm::mat4& CameraSystem::getViewProjectionMat( const std::string& p_layerId )
    {
        if ( !m_layerActiveData.contains( p_layerId ) )
        {
            return m_defaultCameraData.m_viewProjMat;
        }

        return m_layerActiveData[p_layerId].m_viewProjMat;
    }

    const glm::mat4& CameraSystem::getViewProjectionInvMat( const std::string& p_layerId )
    {
        if ( !m_layerActiveData.contains( p_layerId ) )
        {
            return m_defaultCameraData.m_viewProjMatInv;
        }

        return m_layerActiveData[p_layerId].m_viewProjMatInv;
    }

    const glm::mat4& CameraSystem::getViewMat( const std::string& p_layerId )
    {
        if ( !m_layerActiveData.contains( p_layerId ) )
        {
            return m_defaultCameraData.m_viewMat;
        }

        return m_layerActiveData[p_layerId].m_viewMat;
    }

    const glm::mat4& CameraSystem::getViewInvMat( const std::string& p_layerId )
    {
        if ( !m_layerActiveData.contains( p_layerId ) )
        {
            return m_defaultCameraData.m_viewMatInv;
        }

        return m_layerActiveData[p_layerId].m_viewMatInv;
    }
} // namespace Tomos
