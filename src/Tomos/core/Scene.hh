#pragma once

#include "Tomos/events/Event.hh"
#include "Tomos/core/Node.hh"

namespace Tomos
{
    class Scene
    {
    public:
        explicit Scene( const std::string& p_layerId, const std::string& p_name = "UnnamedScene" ) :
            m_root( p_layerId ),
            m_name( p_name )
        {
        }

        virtual ~Scene() = default;

        virtual void update()
        {
            getRoot().computeTransforms();
        }

        virtual void onEvent( Event& p_event )
        {
        }

        inline SceneNode& getRoot() { return m_root; }

        inline const std::string& getName() const { return m_name; }

    protected:
        SceneNode m_root;

        std::string m_name{};
    };

    class SceneManager
    {
    public:
        SceneManager() = default;

        void                   pushScene( const std::shared_ptr<Scene>& p_scene );
        std::shared_ptr<Scene> popScene();

        void operator<<( const std::shared_ptr<Scene>& p_scene );

        const std::shared_ptr<Scene>& activeScene();

    private:
        std::vector<std::shared_ptr<Scene>> scenes;
    };
} // Tomos
