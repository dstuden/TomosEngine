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

        void queueScene( TSceneFactory p_factory );

        void switchPoint();

        void shutdown();

    private:
        std::shared_ptr<TScene> m_scene;
        TSceneFactory           m_switchToScene;
    };
}  // namespace Tomos
