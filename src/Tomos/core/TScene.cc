//
// Created by dstuden on 2/7/25.
//

#include "TScene.hh"

namespace Tomos
{
    void TSceneManager::pushScene( const std::shared_ptr<TScene>& p_scene )
    {
        scenes.push_back( p_scene );
        p_scene->getRoot()->setActive( true );
    }

    std::shared_ptr<TScene> TSceneManager::popScene()
    {
        auto s = scenes.back();
        s->getRoot()->setActive( false );
        scenes.pop_back();
        return s;
    }

    void TSceneManager::operator<<( const std::shared_ptr<TScene>& p_scene )
    {
        pushScene( p_scene );
    }

    const std::shared_ptr<TScene>& TSceneManager::activeScene()
    {
        return scenes.back();
    }
} // namespace Tomos
