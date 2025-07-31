#pragma once

#include "Tomos/core/TNode.hh"
#include "Tomos/events/TEvent.hh"

namespace Tomos
{
    class TScene
    {
    public:
        explicit TScene( const std::string& p_layerId, const std::string& p_name = "UnnamedScene" ) :
            m_root( p_layerId ),
            m_name( p_name )
        {
        }

        virtual ~TScene() = default;

        virtual void update()
        {
            getRoot().computeTransforms();
        }

        virtual void onEvent( TEvent& p_event )
        {
        }

        inline TSceneNode& getRoot() { return m_root; }

        inline const std::string& getName() const { return m_name; }

    protected:
        TSceneNode m_root;

        std::string m_name{};
    };

    class SceneManager
    {
    public:
        SceneManager() = default;

        void                   pushScene( const std::shared_ptr<TScene>& p_scene );
        std::shared_ptr<TScene> popScene();

        void operator<<( const std::shared_ptr<TScene>& p_scene );

        const std::shared_ptr<TScene>& activeScene();

    private:
        std::vector<std::shared_ptr<TScene>> scenes;
    };
} // Tomos
