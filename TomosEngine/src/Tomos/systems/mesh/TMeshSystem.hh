#pragma once

#include <memory>
#include <tuple>
#include <unordered_map>
#include <vector>

#include "Tomos/systems/TSystem.hh"
#include "Tomos/systems/mesh/TMeshComponent.hh"

namespace Tomos
{
    class TAnimatedTextureSystem;
    class TFrameState;
    class TVkMaterial;
    class TVkImage;

    // Blend batches opaque/mask; blend materials stay one instance per draw.
    class TMeshSystem : public TTypedSystem<TMeshComponent>
    {
        friend class TAnimatedTextureSystem;

    public:
        void componentCreated( TSceneNode& p_node, TComponent& p_component ) override;
        void componentDestroyed( TSceneNode& p_node, TComponent& p_component ) override;

        void lateUpdate( float p_dt ) override;

        void populate( TFrameState& p_state ) const;

    private:
        [[nodiscard]] const TVkMaterial* materialFor( const TMeshComponent& p_mc ) const;

        std::unordered_map<TMeshComponent*, TSceneNode*> m_meshes;
        std::vector<TSkinnedMeshComponent*>              m_skinned;

        // Cache of materials with texture overrides. Keyed by base + override images.
        // Mutable so populate() can create clones on demand.
        using TOverrideKey = std::tuple<const TVkMaterial*, const TVkImage*, const TVkImage*>;
        struct TOverrideKeyHash
        {
            size_t operator()( const TOverrideKey& p_k ) const noexcept
            {
                const auto h0 = std::hash<const void*>{}( std::get<0>( p_k ) );
                const auto h1 = std::hash<const void*>{}( std::get<1>( p_k ) );
                const auto h2 = std::hash<const void*>{}( std::get<2>( p_k ) );
                return h0 ^ ( h1 << 1 ) ^ ( h2 << 2 );
            }
        };
        mutable std::unordered_map<TOverrideKey, std::unique_ptr<TVkMaterial>, TOverrideKeyHash> m_overrideMaterials;
    };
}  // namespace Tomos
