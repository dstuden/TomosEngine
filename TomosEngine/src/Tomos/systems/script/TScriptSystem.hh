#pragma once

#include <vector>

#include "Tomos/systems/TSystem.hh"
#include "Tomos/systems/script/TScriptComponent.hh"

namespace Tomos
{
    class TScriptSystem : public TTypedSystem<TScriptComponent>
    {
    public:
        void componentCreated( TSceneNode& p_node, TComponent& p_component ) override;
        void componentDestroyed( TSceneNode& p_node, TComponent& p_component ) override;

        void earlyUpdate( float p_dt ) override;
        void update( float p_dt ) override;
        void lateUpdate( float p_dt ) override;

    private:
        std::vector<TScriptComponent*> m_order;
    };
}  // namespace Tomos
