#include "Tomos/core/ecs/TECS.hh"

#include "Tomos/util/logger/TLogger.hh"

namespace Tomos
{
    void TECS::addSystem( std::unique_ptr<TSystem> p_system ) { m_systems.push_back( std::move( p_system ) ); }

    void TECS::registerComponent( TSceneNode& p_node, TComponent& p_component )
    {
        // Notify every matching system (must stay symmetric with destroyComponent).
        bool matched = false;
        for ( auto& sys : m_systems )
        {
            if ( !sys->matches( p_component ) ) continue;
            sys->componentCreated( p_node, p_component );
            matched = true;
        }
        if ( !matched ) TLOG_WARN() << "Component was not matched by any system\n";
    }

    void TECS::destroyComponent( TSceneNode& p_node, TComponent& p_component )
    {
        for ( auto& sys : m_systems )
        {
            if ( sys->matches( p_component ) ) sys->componentDestroyed( p_node, p_component );
        }
    }

    void TECS::earlyUpdate( float p_dt )
    {
        for ( auto& sys : m_systems ) sys->earlyUpdate( p_dt );
    }

    void TECS::update( float p_dt )
    {
        for ( auto& sys : m_systems ) sys->update( p_dt );
    }

    void TECS::lateUpdate( float p_dt )
    {
        for ( auto& sys : m_systems ) sys->lateUpdate( p_dt );
    }
}  // namespace Tomos
