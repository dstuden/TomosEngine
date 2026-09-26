#pragma once

#include "Tomos/systems/TSystem.hh"

namespace Tomos
{
    class TScene;
    class TVkGpu;

    // Advances scene-bag animated textures each frame (before render populate).
    class TAnimatedTextureSystem : public TSystem
    {
    public:
        void setScene( TScene* p_scene ) { m_scene = p_scene; }
        void setGpu( TVkGpu* p_gpu ) { m_gpu = p_gpu; }

        [[nodiscard]] bool matches( const TComponent& /*p_component*/ ) const override { return false; }

        void update( float p_dt ) override;

    private:
        TScene* m_scene = nullptr;
        TVkGpu* m_gpu   = nullptr;
    };
}  // namespace Tomos
