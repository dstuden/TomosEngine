#pragma once

#include <unordered_map>
#include <vector>

#include "Tomos/systems/TSystem.hh"
#include "Tomos/systems/animation/TAnimatorComponent.hh"

namespace Tomos
{
    class TSceneNode;

    // Runs in update() before computeTransforms.
    class TAnimationSystem : public TTypedSystem<TAnimatorComponent>
    {
    public:
        void componentCreated( TSceneNode& p_node, TComponent& p_component ) override;
        void componentDestroyed( TSceneNode& p_node, TComponent& p_component ) override;

        void update( float p_dt ) override;

    private:
        std::unordered_map<TAnimatorComponent*, TSceneNode*> m_animators;
    };
}  // namespace Tomos
