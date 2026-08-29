#pragma once

#include <glm/gtc/quaternion.hpp>
#include <string>
#include <unordered_map>

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

        // Nested pose type used by .cc sampling helpers (anonymous namespace).
        struct TJointPose
        {
            bool      m_hasT = false;
            bool      m_hasR = false;
            bool      m_hasS = false;
            glm::vec3 m_t{ 0.0f };
            glm::quat m_r{ 1.0f, 0.0f, 0.0f, 0.0f };
            glm::vec3 m_s{ 1.0f };
        };

    private:
        std::unordered_map<TAnimatorComponent*, TSceneNode*>     m_animators;
        std::unordered_map<std::string, TJointPose>              m_fromPose;
        std::unordered_map<std::string, TJointPose>              m_toPose;
    };
}  // namespace Tomos
