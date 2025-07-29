//
// Created by dstuden on 1/20/25.
//

#include "Ecs.hh"

#include "Application.hh"

namespace Tomos
{
    int ECS::registerComponent( const std::shared_ptr<Component>& p_component, const std::shared_ptr<Node>& p_node )
    {
        LOG_DEBUG() << "Start";
        auto t = typeid( *p_component ).hash_code();

        for ( auto& [_, system] : m_systems )
        {
            if ( system->getComponentType().hash_code() == t )
            {
                system->componentAdded( p_component, p_node );
                LOG_DEBUG() << "End";
                return 0;
            }
        }

        for ( auto& [_, system] : m_fixedTimeStepSystems )
        {
            if ( system->getComponentType().hash_code() == t )
            {
                system->componentAdded( p_component, p_node );
                LOG_DEBUG() << "End";
                return 0;
            }
        }

        LOG_WARN() << "End: No system found for component: " << p_component->m_name;
        return -1;
    }

    int ECS::destroyComponent( const std::shared_ptr<Component>& p_component, const std::shared_ptr<Node>& p_node )
    {
        LOG_DEBUG() << "Start";
        auto t = typeid( *p_component ).hash_code();

        for ( auto& [_, system] : m_systems )
        {
            if ( system->getComponentType().hash_code() == t )
            {
                system->componentRemoved( p_component, p_node );
                LOG_DEBUG() << "End";
                return 0;
            }
        }

        for ( auto& [_, system] : m_fixedTimeStepSystems )
        {
            if ( system->getComponentType().hash_code() == t )
            {
                system->componentRemoved( p_component, p_node );
                LOG_DEBUG() << "End";
                return 0;
            }
        }

        LOG_WARN() << "No system found for component: " << p_component->m_name;

        return -1;
    }

    // Cleanup function to move components to their assigned layer after creation
    // TODO: this sould be done in a more efficient way
    void ECS::updateLayerComponents()
    {
        auto unassignedLayer = Application::getState().config().get<std::string>( "unassignedLayerId" );

        for ( auto& [_, system] : m_systems )
        {
            // First collect all components that need to be moved
            std::vector<std::pair<std::shared_ptr<Component>, std::shared_ptr<Node>>> toMove;

            auto& unassignedComponents = system->getComponents()[unassignedLayer];
            for ( auto it = unassignedComponents.begin(); it != unassignedComponents.end(); )
            {
                auto& [component, node] = *it;
                if ( node->getLayerId() != unassignedLayer )
                {
                    toMove.emplace_back( component, node );
                    it = unassignedComponents.erase( it ); // Safe erase while iterating
                }
                else
                {
                    ++it;
                }
            }

            // Then move them to their new layers
            for ( auto& [component, node] : toMove )
            {
                system->m_components[node->getLayerId()][component] = node;
            }
        }
    }

    void ECS::earlyUpdate( const std::string& p_layerId )
    {
        for ( auto& [_, system] : m_systems )
        {
            system->earlyUpdate( p_layerId );
        }
    }

    void ECS::update( const std::string& p_layerId )
    {
        for ( auto& [_, system] : m_systems )
        {
            system->update( p_layerId );
        }
    }

    void ECS::lateUpdate( const std::string& p_layerId )
    {
        for ( auto& [_, system] : m_systems )
        {
            system->lateUpdate( p_layerId );
        }
    }

    void ECS::updateFixedTimeStep( float p_deltaTime, const std::string& p_layerId )
    {
        for ( auto& [_, system] : m_fixedTimeStepSystems )
        {
            system->update( p_deltaTime, p_layerId );
        }
    }
} // namespace Tomos
