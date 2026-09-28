#pragma once

#include <optional>
#include <unordered_map>
#include <vector>

#include "Tomos/systems/TSystem.hh"
#include "Tomos/systems/particle/TParticleEmitterComponent.hh"

namespace Tomos
{
    class TAnimatedTextureSystem;
    class TSceneNode;
    class TVkImage;
    struct TFrameState;

    // Texture slots stable for in-flight GPU particles; overflow → default slot + warn.
    class TParticleSystem : public TTypedSystem<TParticleEmitterComponent>
    {
        friend class TAnimatedTextureSystem;

    public:
        TParticleSystem();

        void componentCreated( TSceneNode& p_node, TComponent& p_component ) override;
        void componentDestroyed( TSceneNode& p_node, TComponent& p_component ) override;

        void populate( TFrameState& p_state, float p_dt );

        [[nodiscard]] size_t emitterCount() const { return m_emitters.size(); }

    private:
        [[nodiscard]] size_t   computeTextureFingerprint() const;
        void                   syncTextureSlots();
        [[nodiscard]] uint32_t textureIndex( const TVkImage* p_texture );

        std::unordered_map<TParticleEmitterComponent*, TSceneNode*> m_emitters;
        std::vector<const TVkImage*>                                m_textures;
        std::unordered_map<const TVkImage*, uint32_t>               m_texToIndex;
        std::vector<uint32_t>                                       m_freeSlots;
        std::optional<size_t>                                       m_lastTextureFingerprint;
        bool                                                        m_overflowWarned = false;
    };
}  // namespace Tomos
