#pragma once

#include <functional>
#include <memory>

#include "Tomos/core/scene/TScene.hh"

namespace Tomos
{
    using TSceneFactory = std::function<std::shared_ptr<TScene>()>;

    // Hard scene replace — no inactive-scene stack.
    class TSceneManager
    {
    public:
        TSceneManager();
        explicit TSceneManager( std::shared_ptr<TScene> p_initialScene );

        [[nodiscard]] TScene&                 scene() { return *m_scene; }
        [[nodiscard]] const TScene&           scene() const { return *m_scene; }
        [[nodiscard]] std::shared_ptr<TScene> scenePtr() const { return m_scene; }

        // When false, scene tick skips ECS update but still refreshes transforms.
        [[nodiscard]] bool isSimulationPlaying() const { return m_simulationPlaying; }
        void               setSimulationPlaying( bool p_playing ) { m_simulationPlaying = p_playing; }

        void queueScene( TSceneFactory p_factory );

        void switchPoint();

        void shutdown();

    private:
        std::shared_ptr<TScene> m_scene;
        TSceneFactory           m_switchToScene;
        bool                    m_simulationPlaying = true;
    };
}  // namespace Tomos
