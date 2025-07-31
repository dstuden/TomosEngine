#pragma once

#include <memory>
#include <unordered_map>

#include "TNode.hh"
#include "Tomos/systems/TSystem.hh"
#include "Tomos/util/logger/TLogger.hh"

namespace Tomos
{
    class ECS
    {
    public:
        template<typename T>
        void registerSystem()
        {
            bool isBase = std::is_base_of_v<TSystem, T>;
            TLOG_ASSERT( isBase );

            if ( m_systems.contains( typeid( T ) ) )
            {
                TLOG_ERROR() << "System already registered: " << typeid( T ).name();
                return;
            }

            m_systems[typeid( T )] = std::make_unique<T>();
            TLOG_INFO() << "System registered: " << typeid( T ).name();
        }

        template<typename T>
        void registerFixedTimeStepSystem( float p_fixedTimeStep = 0.01f )
        {
            bool isBase = std::is_base_of_v<FixedTimeStepSystem, T>;
            TLOG_ASSERT( isBase );

            if ( m_fixedTimeStepSystems.contains( typeid( T ) ) )
            {
                TLOG_ERROR() << "Fixed time step system already registered: " << typeid( T ).name();
                return;
            }

            m_fixedTimeStepSystems[typeid( T )] = std::make_unique<T>( p_fixedTimeStep );
            TLOG_INFO() << "Fixed time step system registered: " << typeid( T ).name();
        }

        int registerComponent( const std::shared_ptr<TComponent>& p_component, const std::shared_ptr<TNode>& p_node );
        int destroyComponent( const std::shared_ptr<TComponent>& p_component, const std::shared_ptr<TNode>& p_node );

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
                TLOG_ERROR() << "System not found: " << type.name();
            }
            return *static_cast<T*>( m_systems[type].get() );
        }

        template<typename T>
        T& getFixedTimeStepSystem()
        {
            std::type_index type = typeid( T );

            if ( m_fixedTimeStepSystems.find( type ) == m_fixedTimeStepSystems.end() )
            {
                TLOG_ERROR() << "Fixed time step system not found: " << type.name();
            }
            return *static_cast<T*>( m_fixedTimeStepSystems[type].get() );
        }

    private:
        std::unordered_map<std::type_index, std::unique_ptr<TSystem>>              m_systems;
        std::unordered_map<std::type_index, std::unique_ptr<FixedTimeStepSystem>> m_fixedTimeStepSystems;
    };
} // namespace Tomos
