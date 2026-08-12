#include "Tomos/systems/animation/TAnimatorComponent.hh"

#include "Tomos/systems/asset/TAssetSystem.hh"

namespace Tomos
{
    bool TAnimatorComponent::rebind( const TAssetSystem& p_assets )
    {
        if ( m_clipRef.empty() ) return m_clip != nullptr;

        if ( m_boundGeneration == p_assets.generation() && m_clip != nullptr ) return true;

        m_clip = p_assets.tryResolve( m_clipRef );
        if ( m_clip == nullptr )
        {
            m_boundGeneration = 0;
            m_playing         = false;
            return false;
        }

        m_boundGeneration = p_assets.generation();
        return true;
    }
}  // namespace Tomos
