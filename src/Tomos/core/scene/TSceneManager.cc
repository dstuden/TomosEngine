#include "TSceneManager.hh"

#include "Tomos/core/layers/TSceneLayer.hh"

namespace Tomos
{
    TSceneManager::TSceneManager() : m_scene( std::make_shared<TScene>( "Main" ) ) {}

    TSceneManager::TSceneManager( std::shared_ptr<TScene> p_initialScene ) : m_scene( std::move( p_initialScene ) ) {}

    void TSceneManager::queueScene( TSceneFactory p_factory ) { m_switchToScene = std::move( p_factory ); }

    void TSceneManager::switchPoint()
    {
        if ( !m_switchToScene ) return;

        m_scene->clearLevel();
        m_scene         = m_switchToScene();
        m_switchToScene = nullptr;
        // Same path as TSceneLayer::onAttach — switched scenes need ECS systems.
        TSceneLayer::registerDefaultSystems( *m_scene );
        m_scene->activate();
    }

    void TSceneManager::shutdown()
    {
        m_switchToScene = nullptr;
        if ( m_scene == nullptr ) return;
        m_scene->clearLevel();
        m_scene->resources().clear();
        // Drop ECS (mesh override materials, etc.) before TVkGpu teardown.
        m_scene.reset();
    }
}  // namespace Tomos
