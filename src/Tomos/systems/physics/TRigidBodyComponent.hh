#pragma once

#include <algorithm>
#include <glm/glm.hpp>

#include "Tomos/systems/TComponent.hh"
#include "Tomos/util/reflect/TReflectAttr.hh"

namespace Tomos
{
    // Do not drive dynamic body transforms from scripts — use forces or teleportTo().
    class TOMOS_ANN( Reflect::ComponentMeta{ "rigidBody", "Rigid Body" } ) TRigidBodyComponent : public TComponent
    {
    public:
        TOMOS_ANN( Reflect::UiLabel{ "Velocity" } ) glm::vec3 m_linearVelocity{ 0.0f };
        TOMOS_ANN( Reflect::Skip{} ) glm::vec3 m_force{ 0.0f };

        TOMOS_ANN( Reflect::UiSkip{} ) float m_mass          = 1.0f;
        TOMOS_ANN( Reflect::Skip{} ) float   m_invMass       = 1.0f;
        TOMOS_ANN( Reflect::UiLabel{ "Gravity scale" } ) TOMOS_ANN( Reflect::UiRange{ 0.0f, 10.0f } ) float m_gravityScale  = 1.0f;
        TOMOS_ANN( Reflect::UiLabel{ "Linear damping" } ) TOMOS_ANN( Reflect::UiRange{ 0.0f, 1.0f } ) float m_linearDamping = 0.05f;
        TOMOS_ANN( Reflect::UiRange{ 0.0f, 1.0f } ) float                                         m_restitution   = 0.0f;

        TOMOS_ANN( Reflect::UiLabel{ "Use gravity" } ) bool m_useGravity = true;
        bool                                        m_kinematic  = false;

        void setMass( float p_mass )
        {
            m_mass    = std::max( p_mass, 0.0f );
            m_invMass = m_mass > 0.0f ? 1.0f / m_mass : 0.0f;
        }

        void addForce( const glm::vec3& p_force ) { m_force += p_force; }

        void clearForces() { m_force = glm::vec3( 0.0f ); }

        void teleportTo( const glm::vec3& p_position )
        {
            m_pendingTeleport    = p_position;
            m_hasPendingTeleport = true;
        }

        [[nodiscard]] bool isDynamic() const { return !m_kinematic && m_invMass > 0.0f; }

        [[nodiscard]] bool hasPendingTeleport() const { return m_hasPendingTeleport; }

        [[nodiscard]] glm::vec3 takePendingTeleport()
        {
            m_hasPendingTeleport = false;
            return m_pendingTeleport;
        }

    private:
        glm::vec3 m_pendingTeleport{ 0.0f };
        bool      m_hasPendingTeleport = false;
    };
}  // namespace Tomos
