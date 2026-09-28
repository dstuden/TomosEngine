#pragma once

#include <unordered_map>

#include "Tomos/systems/TSystem.hh"
#include "Tomos/systems/sprite/TSpriteComponent.hh"

namespace Tomos
{
    class TAnimatedTextureSystem;
    class TSceneNode;
    struct TFrameState;

    class TSpriteSystem : public TTypedSystem<TSpriteComponent>
    {
        friend class TAnimatedTextureSystem;

    public:
        void componentCreated( TSceneNode& p_node, TComponent& p_component ) override;
        void componentDestroyed( TSceneNode& p_node, TComponent& p_component ) override;

        void populate( TFrameState& p_state ) const;

    private:
        std::unordered_map<TSpriteComponent*, TSceneNode*> m_sprites;
    };
}  // namespace Tomos
