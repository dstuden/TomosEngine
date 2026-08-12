#pragma once

#include <unordered_map>

#include "Tomos/systems/TSystem.hh"
#include "Tomos/systems/light/TLightComponent.hh"

namespace Tomos
{
    class TFrameState;

    class TLightSystem : public TTypedSystem<TLightComponent>
    {
    public:
        void componentCreated( TSceneNode& p_node, TComponent& p_component ) override;
        void componentDestroyed( TSceneNode& p_node, TComponent& p_component ) override;

        void populate( TFrameState& p_state ) const;

    private:
        std::unordered_map<TLightComponent*, TSceneNode*> m_lights;
    };
}  // namespace Tomos
