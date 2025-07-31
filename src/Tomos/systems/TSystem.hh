#pragma once

#include <typeindex>

#include "TComponent.hh"
#include "Tomos/core/TNode.hh"

namespace Tomos
{
    class TSystem
    {
        friend class ECS;

    public:
        TSystem() {}

        virtual ~TSystem() = default;

        virtual void earlyUpdate( const std::string& p_layerId ) {};

        virtual void update( const std::string& p_layerId ) {};

        virtual void lateUpdate( const std::string& p_layerId ) {};

        virtual void componentAdded( const std::shared_ptr<TComponent>& p_component, const std::shared_ptr<TNode>& p_node ) {};

        virtual void componentRemoved( const std::shared_ptr<TComponent>& p_component, const std::shared_ptr<TNode>& p_node ) {};

        virtual std::type_index getComponentType() const { return typeid( nullptr ); };

        std::unordered_map<std::string, std::unordered_map<std::shared_ptr<TComponent>, std::shared_ptr<TNode>>>& getComponents() { return m_components; }

    protected:
        // Component, Node pairs for each layer
        std::unordered_map<std::string, std::unordered_map<std::shared_ptr<TComponent>, std::shared_ptr<TNode>>> m_components;
    };

    class FixedTimeStepSystem : public TSystem
    {
    public:
        FixedTimeStepSystem( float p_fixedTimeStep = 0.01f ) : m_fixedTimeStep( p_fixedTimeStep ) {}

        virtual void update( float p_deltaTime, const std::string& p_layerId )
        {
            m_accumulator += p_deltaTime;

            while ( m_accumulator >= m_fixedTimeStep )
            {
                m_accumulator -= m_fixedTimeStep;
                TSystem::earlyUpdate( p_layerId );
                TSystem::update( p_layerId );
                TSystem::lateUpdate( p_layerId );
            }
        }

    protected:
        float m_fixedTimeStep = 0.01;  // 100Hz
        float m_accumulator   = 0.0f;
    };
}  // namespace Tomos
