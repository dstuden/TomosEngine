#pragma once

#include <unordered_map>
#include <vector>

#include "Tomos/systems/TSystem.hh"
#include "Tomos/systems/mesh/TMeshComponent.hh"

namespace Tomos
{
    class TFrameState;

    // Blend batches opaque/mask; blend materials stay one instance per draw.
    class TMeshSystem : public TTypedSystem<TMeshComponent>
    {
    public:
        void componentCreated( TSceneNode& p_node, TComponent& p_component ) override;
        void componentDestroyed( TSceneNode& p_node, TComponent& p_component ) override;

        void lateUpdate( float p_dt ) override;

        void populate( TFrameState& p_state ) const;

    private:
        std::unordered_map<TMeshComponent*, TSceneNode*> m_meshes;
        std::vector<TSkinnedMeshComponent*>              m_skinned;
    };
}  // namespace Tomos
