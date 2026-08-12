#include "TSceneManager.hh"

namespace Tomos
{
    TSceneManager::TSceneManager() : m_scene( std::make_shared<TScene>( "Main" ) ) {}

    TSceneManager::TSceneManager( std::shared_ptr<TScene> p_initialScene ) : m_scene( std::move( p_initialScene ) ) {}

    void TSceneManager::queueScene( TSceneFactory p_factory ) { m_switchToScene = std::move( p_factory ); }

    void TSceneManager::switchPoint()
    {
        if ( !m_switchToScene ) return;

        m_scene->deactivate();
        m_scene         = m_switchToScene();
        m_switchToScene = nullptr;
        m_scene->activate();
    }

    void TSceneManager::shutdown()
    {
        m_switchToScene = nullptr;
        if ( m_scene == nullptr ) return;
        if ( m_scene->isActive() ) m_scene->deactivate();
        m_scene->clearChildren();
        m_scene->resources().clear();
    }
}  // namespace Tomos
