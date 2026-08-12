#include "Tomos/systems/mesh/TMeshComponent.hh"

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
}  // namespace Tomos
