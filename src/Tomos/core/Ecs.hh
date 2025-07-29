#pragma once

#include <memory>
#include <unordered_map>

#include "Node.hh"
#include "Tomos/systems/System.hh"
#include "Tomos/util/logger/Logger.hh"

namespace Tomos
{
    class ECS
    {
    public:
        template<typename T>
        void registerSystem()
        {
            bool isBase = std::is_base_of_v<System, T>;
            LOG_ASSERT( isBase );

            if ( m_systems.contains( typeid( T ) ) )
            {
                LOG_ERROR() << "System already registered: " << typeid( T ).name();
                return;
            }

            m_systems[typeid( T )] = std::make_unique<T>();
            LOG_INFO() << "System registered: " << typeid( T ).name();
        }

        template<typename T>
        void registerFixedTimeStepSystem( float p_fixedTimeStep = 0.01f )
        {
            bool isBase = std::is_base_of_v<FixedTimeStepSystem, T>;
            LOG_ASSERT( isBase );

            if ( m_fixedTimeStepSystems.contains( typeid( T ) ) )
            {
                LOG_ERROR() << "Fixed time step system already registered: " << typeid( T ).name();
                return;
            }

            m_fixedTimeStepSystems[typeid( T )] = std::make_unique<T>( p_fixedTimeStep );
            LOG_INFO() << "Fixed time step system registered: " << typeid( T ).name();
        }

        int registerComponent( const std::shared_ptr<Component>& p_component, const std::shared_ptr<Node>& p_node );
        int destroyComponent( const std::shared_ptr<Component>& p_component, const std::shared_ptr<Node>& p_node );

        void updateLayerComponents();

        void earlyUpdate( const std::string& p_layerId );
        void update( const std::string& p_layerId );
        void lateUpdate( const std::string& p_layerId );

        void updateFixedTimeStep( float p_deltaTime, const std::string& p_layerId );

        template<typename T>
        T& getSystem()
        {
            std::type_index type = typeid( T );

            if ( m_systems.find( type ) == m_systems.end() )
            {
                LOG_ERROR() << "System not found: " << type.name();
            }
            return *static_cast<T*>( m_systems[type].get() );
        }

        template<typename T>
        T& getFixedTimeStepSystem()
        {
            std::type_index type = typeid( T );

            if ( m_fixedTimeStepSystems.find( type ) == m_fixedTimeStepSystems.end() )
            {
                LOG_ERROR() << "Fixed time step system not found: " << type.name();
            }
            return *static_cast<T*>( m_fixedTimeStepSystems[type].get() );
        }

    private:
        std::unordered_map<std::type_index, std::unique_ptr<System>>              m_systems;
        std::unordered_map<std::type_index, std::unique_ptr<FixedTimeStepSystem>> m_fixedTimeStepSystems;
    };
} // namespace Tomos
