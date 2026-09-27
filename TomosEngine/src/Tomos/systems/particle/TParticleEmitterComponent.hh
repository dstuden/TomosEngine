#pragma once

#include <algorithm>
#include <cstdint>
#include <glm/glm.hpp>

#include "Tomos/systems/TComponent.hh"
#include "Tomos/systems/asset/TAssetHandles.hh"
#include "Tomos/util/reflect/TReflectAttr.hh"

namespace Tomos
{
    class TVkImage;

    class TOMOS_ANN( Reflect::ComponentMeta{ "particleEmitter", "Particle Emitter" } ) TParticleEmitterComponent : public TComponent
    {
    public:
        bool m_emitting = true;

        TOMOS_ANN( Reflect::UiRange{ 0.0f, 400.0f } ) float m_rate = 50.0f;

        TOMOS_ANN( Reflect::UiLabel{ "Life min" } ) TOMOS_ANN( Reflect::UiRange{ 0.05f, 3.0f } ) float m_lifetimeMin = 0.4f;
        TOMOS_ANN( Reflect::UiLabel{ "Life max" } ) TOMOS_ANN( Reflect::UiRange{ 0.05f, 4.0f } ) float m_lifetimeMax = 1.2f;

        TOMOS_ANN( Reflect::UiLabel{ "Vel min" } ) glm::vec3 m_velocityMin = { -0.5f, 1.0f, -0.5f };
        TOMOS_ANN( Reflect::UiLabel{ "Vel max" } ) glm::vec3 m_velocityMax = { 0.5f, 3.0f, 0.5f };

        TOMOS_ANN( Reflect::UiLabel{ "Size start" } ) glm::vec2 m_sizeStart = { 0.15f, 0.15f };
        TOMOS_ANN( Reflect::UiLabel{ "Size end" } ) glm::vec2   m_sizeEnd   = { 0.02f, 0.02f };

        TOMOS_ANN( Reflect::UiColor{} ) TOMOS_ANN( Reflect::UiLabel{ "Color start" } ) glm::vec4 m_colorStart = { 1.0f, 0.7f, 0.2f, 1.0f };
        TOMOS_ANN( Reflect::UiColor{} ) TOMOS_ANN( Reflect::UiLabel{ "Color end" } ) glm::vec4   m_colorEnd   = { 1.0f, 0.1f, 0.0f, 0.0f };

        TOMOS_ANN( Reflect::UiRange{ -20.0f, 5.0f } ) float m_gravity = -2.0f;

        TOMOS_ANN( Reflect::Skip{} ) const TVkImage* m_texture = nullptr;
        TOMOS_ANN( Reflect::UiLabel{ "UV min" } ) glm::vec2 m_uvMin = { 0.0f, 0.0f };
        TOMOS_ANN( Reflect::UiLabel{ "UV max" } ) glm::vec2 m_uvMax = { 1.0f, 1.0f };

        TOMOS_ANN( Reflect::Skip{} ) TBagAnimatedTextureRef m_animRef{};

        void burst( uint32_t p_count ) { m_pendingBurst += p_count; }

        TOMOS_ANN( Reflect::Skip{} ) float    m_emitAccum    = 0.0f;
        TOMOS_ANN( Reflect::Skip{} ) uint32_t m_pendingBurst = 0;
        TOMOS_ANN( Reflect::UiRange{ 1.0f, 1000000.0f } ) uint32_t m_seed = 1;

        static constexpr uint32_t g_kMaxSpawnPerFrame = 1024;

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

            return std::min( count, g_kMaxSpawnPerFrame );
        }
    };
}  // namespace Tomos
