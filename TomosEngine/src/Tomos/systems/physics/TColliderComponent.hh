#pragma once

#include <cstdint>
#include <functional>
#include <glm/glm.hpp>

#include "Tomos/systems/TComponent.hh"
#include "Tomos/systems/physics/TPhysicsLayers.hh"

namespace Tomos
{
    class TSceneNode;

    enum class TColliderShape : int
    {
        Sphere = 0,
        Box    = 1,  // OBB (node world rotation)
    };

    // Trigger pair lifecycle (TPhysicsSystem fixed step).
    enum class TOverlapPhase : int
    {
        Enter = 0,
        Stay  = 1,
        Exit  = 2,
    };

    using TOverlapCallback = std::function<void( TSceneNode& p_self, TSceneNode& p_other, TOverlapPhase p_phase )>;

    // World pose from node transform. m_radius/m_halfExtents are local (× scale).
    class TColliderComponent : public TComponent
    {
    public:
        TColliderShape m_shape = TColliderShape::Box;

        float     m_radius      = 0.5f;
        glm::vec3 m_halfExtents = { 0.5f, 0.5f, 0.5f };

        bool m_isTrigger = false;
        bool m_enabled   = true;

        uint32_t m_layer = TPhysicsLayer::Default;
        uint32_t m_mask  = TPhysicsLayer::All;

        TOverlapCallback m_onOverlap;

        [[nodiscard]] bool interactsWith( const TColliderComponent& p_other ) const
        {
            return ( m_layer & p_other.m_mask ) != 0 && ( p_other.m_layer & m_mask ) != 0;
        }
    };
}  // namespace Tomos
