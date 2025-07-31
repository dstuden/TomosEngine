#pragma once
#include "TScriptComponent.hh"
#include "Tomos/systems/TSystem.hh"

namespace Tomos
{
    class TScriptSystem : public TSystem
    {
    public:
        TScriptSystem()
        {
        }

        void componentAdded( const std::shared_ptr<TComponent>& p_component, const std::shared_ptr<TNode>& p_node ) override;
        void componentRemoved( const std::shared_ptr<TComponent>& p_component, const std::shared_ptr<TNode>& p_node ) override;

        void earlyUpdate( const std::string& p_layerId ) override;
        void update( const std::string& p_layerId ) override;
        void lateUpdate( const std::string& p_layerId ) override;

        std::type_index getComponentType() const override { return typeid( TScriptComponent ); }
    };
} // Tomos
