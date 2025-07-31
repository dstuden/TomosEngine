#pragma once
#include "Tomos/systems/TSystem.hh"

namespace Tomos
{
    class ColliderComponent : public TComponent
    {
    public:
        std::string m_name = "ColliderComponent";
    };

    class TPhysicsSystem : public FixedTimeStepSystem
    {
    public:
        TPhysicsSystem( float p_fixedTimeStep = 0.01f ) :
            FixedTimeStepSystem( p_fixedTimeStep )
        {
            initJolt();
        }

        std::type_index getComponentType() const override { return typeid( ColliderComponent ); }

        void componentAdded( const std::shared_ptr<TComponent>& p_component, const std::shared_ptr<TNode>& p_node ) override;
        void componentRemoved( const std::shared_ptr<TComponent>& p_component, const std::shared_ptr<TNode>& p_node ) override;

        void update( const std::string& p_layerId ) override;

        void update( float p_deltaTime, const std::string& p_layerId ) override;

    private:
        void initJolt();

        float m_fixedTimeStep   = 0.01f;
        int   m_maxPhysicsSteps = 3;
    };
} // Tomos
