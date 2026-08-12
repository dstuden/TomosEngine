#pragma once

#include <memory>
#include <stdexcept>
#include <vector>

#include "Tomos/systems/TSystem.hh"

namespace Tomos
{
    class TSceneNode;

    class TECS
    {
    public:
        void addSystem( std::unique_ptr<TSystem> p_system );

        void registerComponent( TSceneNode& p_node, TComponent& p_component );
        void destroyComponent( TSceneNode& p_node, TComponent& p_component );

        void earlyUpdate( float p_dt );
        void update( float p_dt );
        void lateUpdate( float p_dt );

        template<typename T>
        T& getSystem()
        {
            for ( auto& sys : m_systems )
            {
                if ( auto* ptr = dynamic_cast<T*>( sys.get() ) ) return *ptr;
            }
            throw std::runtime_error( "System not found in ECS" );
        }

        template<typename T>
        T* maybeGetSystem()
        {
            for ( auto& sys : m_systems )
            {
                if ( auto* ptr = dynamic_cast<T*>( sys.get() ) ) return ptr;
            }
            return nullptr;
        }

    private:
        std::vector<std::unique_ptr<TSystem>> m_systems;
    };
}  // namespace Tomos
