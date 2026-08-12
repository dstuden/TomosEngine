#pragma once

#include <cstdint>
#include <glm/glm.hpp>
#include <unordered_map>

#include "Tomos/systems/TSystem.hh"
#include "Tomos/systems/camera/TCameraComponent.hh"

namespace Tomos
{
    class TFrameState;

    // Lowest active node id wins. Call populate() before render.
    class TCameraSystem : public TTypedSystem<TCameraComponent>
    {
    public:
        void componentCreated( TSceneNode& p_node, TComponent& p_component ) override;
        void componentDestroyed( TSceneNode& p_node, TComponent& p_component ) override;

        void lateUpdate( float p_dt ) override;

        void populate( TFrameState& p_state, float p_aspect ) const;

        [[nodiscard]] bool              hasActiveCamera() const { return m_active != nullptr; }
        [[nodiscard]] TCameraComponent* activeCamera() const { return m_active; }
        [[nodiscard]] TSceneNode*       activeCameraNode() const { return m_activeNode; }

    private:
        std::unordered_map<TCameraComponent*, TSceneNode*> m_cameras;
        TCameraComponent*                                  m_active     = nullptr;
        TSceneNode*                                        m_activeNode = nullptr;
    };
}  // namespace Tomos
