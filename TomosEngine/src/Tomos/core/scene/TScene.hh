#pragma once

#include "Tomos/core/scene/TSceneNode.hh"
#include "Tomos/core/scene/TSceneResourceBag.hh"

namespace Tomos
{
    class TScene : public TSceneNode
    {
    public:
        TScene() = default;
        explicit TScene( std::string p_name );
        ~TScene() override;

        TECS& ecs() { return m_ecsInstance; }

        [[nodiscard]] TSceneResourceBag&       resources() { return m_resources; }
        [[nodiscard]] const TSceneResourceBag& resources() const { return m_resources; }

        void activate();
        void deactivate();
        void computeTransforms();

        [[nodiscard]] bool isActive() const { return m_active; }

        // When false, skips ECS update but still refreshes transforms (editor edits).
        [[nodiscard]] bool isSimulationPlaying() const { return m_simulationPlaying; }
        void               setSimulationPlaying( bool p_playing ) { m_simulationPlaying = p_playing; }

    private:
        struct TTransformTask
        {
            TSceneNode*       m_node;
            const TTransform* m_parentTransform;
            bool              m_parentDirty;
        };

        TECS              m_ecsInstance;
        TSceneResourceBag m_resources;
        bool              m_active            = false;
        bool              m_simulationPlaying = true;
    };
}  // namespace Tomos
