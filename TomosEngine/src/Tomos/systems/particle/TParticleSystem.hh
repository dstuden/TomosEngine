#pragma once

#include <unordered_map>
#include <vector>

#include "Tomos/systems/TSystem.hh"
#include "Tomos/systems/particle/TParticleEmitterComponent.hh"

namespace Tomos
{
    class TSceneNode;
    class TVkImage;
    struct TFrameState;

    // Texture slots stable for in-flight GPU particles; overflow → default slot + warn.
    class TParticleSystem : public TTypedSystem<TParticleEmitterComponent>
    {
    public:
        TParticleSystem();

        void componentCreated( TSceneNode& p_node, TComponent& p_component ) override;
        void componentDestroyed( TSceneNode& p_node, TComponent& p_component ) override;

        void populate( TFrameState& p_state, float p_dt );

        [[nodiscard]] size_t emitterCount() const { return m_emitters.size(); }

    private:
        void                   syncTextureSlots();
        [[nodiscard]] uint32_t textureIndex( const TVkImage* p_texture );

        std::unordered_map<TParticleEmitterComponent*, TSceneNode*> m_emitters;
        std::vector<const TVkImage*>                                m_textures;
        std::unordered_map<const TVkImage*, uint32_t>               m_texToIndex;
        std::vector<uint32_t>                                       m_freeSlots;
        bool                                                        m_overflowWarned = false;
    };
}  // namespace Tomos
