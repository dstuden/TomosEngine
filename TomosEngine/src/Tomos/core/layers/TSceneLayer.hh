#pragma once

#include "Tomos/core/layers/TLayer.hh"
#include "Tomos/core/scene/TScene.hh"

namespace Tomos
{
    // Engine scene runtime: registers default systems, ticks ECS, populates GPU, renders.
    // Game apps subclass for setup (spawn / hooks) only — not for frame orchestration.
    class TSceneLayer : public TLayer
    {
    public:
        explicit TSceneLayer( std::string p_name = "Scene" );

        void onAttach() override;
        void onDetach() override;
        void onUpdate( float p_dt ) override;
        void onRender() override;
        void onEvent( TEvent& p_event ) override;

        [[nodiscard]] static TScene& scene();

        template<typename T>
        [[nodiscard]] T& system()
        {
            return scene().ecs().getSystem<T>();
        }

        static void registerDefaultSystems( TScene& p_scene );

    private:
        float m_lastDt = 0.0f;
    };
}  // namespace Tomos
