//
// Created by dstuden on 3/5/25.
//

#include "TScriptSystem.hh"

#include "Tomos/util/logger/TLogger.hh"

namespace Tomos
{
    void TScriptSystem::componentAdded( const std::shared_ptr<TComponent>& p_component, const std::shared_ptr<TNode>& p_node )
    {
        auto sc = std::dynamic_pointer_cast<TScriptComponent>( p_component );
        if ( m_components.contains( p_node->getLayerId() ) && !m_components[p_node->getLayerId()].erase( p_component ) )
        {
            TLOG_DEBUG() << "Component moved to new node: " << p_component->m_name;
        }
        m_components[p_node->getLayerId()][p_component] = p_node;
        sc->getScript()->m_node                         = p_node;
        sc->getScript()->onAttach();
    }

    void TScriptSystem::componentRemoved( const std::shared_ptr<TComponent>& p_component, const std::shared_ptr<TNode>& p_node )
    {
        if ( m_components.contains( p_node->getLayerId() ) )
        {
            auto sc = std::dynamic_pointer_cast<TScriptComponent>( p_component );
            m_components[p_node->getLayerId()].erase( p_component );
            sc->getScript()->onDetach();
        }
    }

    void TScriptSystem::earlyUpdate( const std::string& p_layerId )
    {
        if ( !m_components.contains( p_layerId ) )
        {
            return;
        }

        for ( auto& [component, node] : m_components[p_layerId] )
        {
            if ( !node->isActive() ) continue;

            auto script = std::dynamic_pointer_cast<TScriptComponent>( component )->getScript();
            script->earlyUpdate();
        }
    }

    void TScriptSystem::update( const std::string& p_layerId )
    {
        if ( !m_components.contains( p_layerId ) )
        {
            return;
        }

        for ( auto& [component, node] : m_components[p_layerId] )
        {
            auto script = std::dynamic_pointer_cast<TScriptComponent>( component )->getScript();
            script->update();
        }
    }

    void TScriptSystem::lateUpdate( const std::string& p_layerId )
    {
        if ( !m_components.contains( p_layerId ) )
        {
            return;
        }

        for ( auto& [component, node] : m_components[p_layerId] )
        {
            auto script = std::dynamic_pointer_cast<TScriptComponent>( component )->getScript();
            script->lateUpdate();
        }
    }
} // Tomos
