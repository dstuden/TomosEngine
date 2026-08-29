#include "Tomos/systems/mesh/TMeshComponent.hh"

#include "Tomos/core/scene/TSceneResourceBag.hh"
#include "Tomos/gpu/vulkan/TVkGpu.hh"
#include "Tomos/systems/asset/TAssetSystem.hh"

namespace Tomos
{
    bool TMeshComponent::rebind( const TAssetSystem& p_assets )
    {
        if ( m_ref.empty() ) return m_mesh != nullptr;

        if ( m_boundGeneration == p_assets.generation() && m_mesh != nullptr ) return true;

        const TVkMesh*     mesh = nullptr;
        const TVkMaterial* mat  = nullptr;
        if ( !p_assets.tryResolve( m_ref, mesh, mat ) )
        {
            m_mesh            = nullptr;
            m_material        = nullptr;
            m_boundGeneration = 0;
            return false;
        }

        m_mesh            = mesh;
        m_material        = mat;
        m_boundGeneration = p_assets.generation();
        return true;
    }

    void TMeshComponent::rebindOverrides( TSceneResourceBag& p_bag, TVkGpu& p_gpu )
    {
        m_baseOverrideImage     = nullptr;
        m_emissionOverrideImage = nullptr;

        if ( !m_baseTextureOverride.empty() )
        {
            TBagAnimatedTextureRef opts = m_baseTextureOverride;
            m_baseOverrideImage          = p_bag.resolveTexture( p_gpu, opts.m_path, opts );
            if ( auto* anim = p_bag.findAnimatedTexture( m_baseOverrideImage ) ) anim->applyOpts( opts );
        }
        if ( !m_emissionTextureOverride.empty() )
        {
            TBagAnimatedTextureRef opts = m_emissionTextureOverride;
            m_emissionOverrideImage      = p_bag.resolveTexture( p_gpu, opts.m_path, opts );
            if ( auto* anim = p_bag.findAnimatedTexture( m_emissionOverrideImage ) ) anim->applyOpts( opts );
        }
    }
}  // namespace Tomos
