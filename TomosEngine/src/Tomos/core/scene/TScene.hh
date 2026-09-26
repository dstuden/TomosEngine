#pragma once

#include <string>

#include "Tomos/core/scene/TSceneNode.hh"
#include "Tomos/core/scene/TSceneResourceBag.hh"
#include "Tomos/util/memory/TLevelStore.hh"

namespace Tomos
{
    // Unpooled scene root; owns the TLevelStore for all other nodes/components.
    class TScene : public TSceneNode
    {
    public:
        TScene();
        explicit TScene( std::string p_name );
        ~TScene() override;

        TECS& ecs() { return m_ecsInstance; }

        [[nodiscard]] TSceneResourceBag&       resources() { return m_resources; }
        [[nodiscard]] const TSceneResourceBag& resources() const { return m_resources; }

        [[nodiscard]] TLevelStore&       store() { return m_levelStore; }
        [[nodiscard]] const TLevelStore& store() const { return m_levelStore; }

        TSceneNode& createNode( std::string p_name = "<unnamed>" );

        void activate();
        void deactivate();
        void computeTransforms();

        // Destroy all pooled nodes/components; root survives empty.
        void clearLevel();

        [[nodiscard]] bool isActive() const { return m_active; }

    private:
        struct TTransformTask
        {
            TSceneNode*       m_node;
            const TTransform* m_parentTransform;
            bool              m_parentDirty;
        };

        TECS              m_ecsInstance;
        TSceneResourceBag m_resources;
        TLevelStore       m_levelStore;
        bool              m_active = false;
    };
}  // namespace Tomos
