#include "Tomos/systems/script/TScriptSystem.hh"

#include <algorithm>

namespace Tomos
{
    void TScriptSystem::componentCreated( TSceneNode& p_node, TComponent& p_component )
    {
        auto& sc = static_cast<TScriptComponent&>( p_component );

        sc.script().m_node = &p_node;
        sc.script().onAttach();

        m_order.push_back( &sc );
    }

    void TScriptSystem::componentDestroyed( TSceneNode& /*p_node*/, TComponent& p_component )
    {
        auto& sc = static_cast<TScriptComponent&>( p_component );

        sc.script().onDetach();
        sc.script().m_node = nullptr;

        m_order.erase( std::remove( m_order.begin(), m_order.end(), &sc ), m_order.end() );
    }

    void TScriptSystem::earlyUpdate( float p_dt )
    {
        for ( auto* sc : m_order ) sc->script().earlyUpdate( p_dt );
    }

    void TScriptSystem::update( float p_dt )
    {
        for ( auto* sc : m_order ) sc->script().update( p_dt );
    }

    void TScriptSystem::lateUpdate( float p_dt )
    {
        for ( auto* sc : m_order ) sc->script().lateUpdate( p_dt );
    }
}  // namespace Tomos
