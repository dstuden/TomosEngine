#include "Tomos/systems/script/TScriptSystem.hh"

#include <algorithm>

namespace Tomos
{
    void TScriptSystem::componentCreated( TSceneNode& p_node, TComponent& p_component )
    {
        auto& sc = dynamic_cast<TScriptComponent&>( p_component );

        sc.script().m_node = &p_node;
        sc.script().onAttach();

        m_order.push_back( &sc );
    }

    void TScriptSystem::componentDestroyed( TSceneNode& /*p_node*/, TComponent& p_component )
    {
        auto& sc = dynamic_cast<TScriptComponent&>( p_component );

        sc.script().onDetach();
        sc.script().m_node = nullptr;

        m_order.erase( std::remove( m_order.begin(), m_order.end(), &sc ), m_order.end() );
    }

    void TScriptSystem::earlyUpdate( float p_dt )
    {
        // Snapshot so destroy mid-phase cannot invalidate the live iterator / UAF.
        const std::vector<TScriptComponent*> snapshot = m_order;
        for ( auto* sc : snapshot )
        {
            if ( std::find( m_order.begin(), m_order.end(), sc ) == m_order.end() ) continue;
            sc->script().earlyUpdate( p_dt );
        }
    }

    void TScriptSystem::update( float p_dt )
    {
        const std::vector<TScriptComponent*> snapshot = m_order;
        for ( auto* sc : snapshot )
        {
            if ( std::find( m_order.begin(), m_order.end(), sc ) == m_order.end() ) continue;
            sc->script().update( p_dt );
        }
    }

    void TScriptSystem::lateUpdate( float p_dt )
    {
        const std::vector<TScriptComponent*> snapshot = m_order;
        for ( auto* sc : snapshot )
        {
            if ( std::find( m_order.begin(), m_order.end(), sc ) == m_order.end() ) continue;
            sc->script().lateUpdate( p_dt );
        }
    }
}  // namespace Tomos
