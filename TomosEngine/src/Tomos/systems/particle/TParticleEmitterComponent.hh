#pragma once

#include <algorithm>
#include <cstdint>
#include <glm/glm.hpp>

#include "Tomos/systems/TComponent.hh"

namespace Tomos
{
    class TVkImage;

    class TParticleEmitterComponent : public TComponent
    {
    public:
        bool m_emitting = true;

        float m_rate = 50.0f;

        float m_lifetimeMin = 0.4f;
        float m_lifetimeMax = 1.2f;

        glm::vec3 m_velocityMin = { -0.5f, 1.0f, -0.5f };
        glm::vec3 m_velocityMax = { 0.5f, 3.0f, 0.5f };

        glm::vec2 m_sizeStart = { 0.15f, 0.15f };
        glm::vec2 m_sizeEnd   = { 0.02f, 0.02f };

        glm::vec4 m_colorStart = { 1.0f, 0.7f, 0.2f, 1.0f };
        glm::vec4 m_colorEnd   = { 1.0f, 0.1f, 0.0f, 0.0f };

        float m_gravity = -2.0f;

        const TVkImage* m_texture = nullptr;
        glm::vec2       m_uvMin   = { 0.0f, 0.0f };
        glm::vec2       m_uvMax   = { 1.0f, 1.0f };

        void burst( uint32_t p_count ) { m_pendingBurst += p_count; }

        float    m_emitAccum    = 0.0f;
        uint32_t m_pendingBurst = 0;
        uint32_t m_seed         = 1;

        static constexpr uint32_t k_maxSpawnPerFrame = 1024;

        [[nodiscard]] uint32_t takeSpawnCount( float p_dt )
        {
            uint32_t count = m_pendingBurst;
            m_pendingBurst = 0;

            if ( m_emitting && m_rate > 0.0f && p_dt > 0.0f )
            {
                m_emitAccum += m_rate * p_dt;
                const auto fromRate = static_cast<uint32_t>( m_emitAccum );
                m_emitAccum -= static_cast<float>( fromRate );
                count += fromRate;
            }

            return std::min( count, k_maxSpawnPerFrame );
        }
    };
}  // namespace Tomos
