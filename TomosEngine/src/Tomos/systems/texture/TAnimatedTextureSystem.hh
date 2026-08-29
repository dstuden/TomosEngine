#pragma once

#include "Tomos/systems/TSystem.hh"

namespace Tomos
{
    // Advances scene-bag animated textures each frame (before render populate).
    class TAnimatedTextureSystem : public TSystem
    {
    public:
        [[nodiscard]] bool matches( const TComponent& /*p_component*/ ) const override { return false; }

        void update( float p_dt ) override;
    };
}  // namespace Tomos
