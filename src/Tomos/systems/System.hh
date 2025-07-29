#pragma once

#include <typeindex>

#include "Component.hh"
#include "Tomos/core/Node.hh"

namespace Tomos
{
    class System
    {
        friend class ECS;

    public:
        System() {}

        virtual ~System() = default;

        virtual void earlyUpdate( const std::string& p_layerId ) {};

        virtual void update( const std::string& p_layerId ) {};

        virtual void lateUpdate( const std::string& p_layerId ) {};

        virtual void componentAdded( const std::shared_ptr<Component>& p_component, const std::shared_ptr<Node>& p_node ) {};

        virtual void componentRemoved( const std::shared_ptr<Component>& p_component, const std::shared_ptr<Node>& p_node ) {};

        virtual std::type_index getComponentType() const { return typeid( nullptr ); };

        std::unordered_map<std::string, std::unordered_map<std::shared_ptr<Component>, std::shared_ptr<Node>>>& getComponents() { return m_components; }

    protected:
        // Component, Node pairs for each layer
        std::unordered_map<std::string, std::unordered_map<std::shared_ptr<Component>, std::shared_ptr<Node>>> m_components;
    };

    class FixedTimeStepSystem : public System
    {
    public:
        FixedTimeStepSystem( float p_fixedTimeStep = 0.01f ) : m_fixedTimeStep( p_fixedTimeStep ) {}

        virtual void update( float p_deltaTime, const std::string& p_layerId )
        {
            m_accumulator += p_deltaTime;

            while ( m_accumulator >= m_fixedTimeStep )
            {
                m_accumulator -= m_fixedTimeStep;
                System::earlyUpdate( p_layerId );
                System::update( p_layerId );
                System::lateUpdate( p_layerId );
            }
        }

    protected:
        float m_fixedTimeStep = 0.01;  // 100Hz
        float m_accumulator   = 0.0f;
    };
}  // namespace Tomos
