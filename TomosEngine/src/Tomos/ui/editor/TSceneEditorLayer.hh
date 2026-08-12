#pragma once

#include <memory>
#include <string>

#include "Tomos/ui/TUiLayer.hh"
#include "Tomos/ui/editor/TSceneEditorContext.hh"

namespace Tomos
{
    // Docked ImGui editor: Scene viewport, hierarchy, inspector, renderer/physics/console.
    class TSceneEditorLayer : public TUiLayer
    {
    public:
        TSceneEditorLayer( TScene& p_scene, std::string p_scenePath = "assets/scenes/sandbox.json" );
        ~TSceneEditorLayer() override;

        [[nodiscard]] TSceneEditorContext&       context() { return m_ctx; }
        [[nodiscard]] const TSceneEditorContext& context() const { return m_ctx; }

    protected:
        void onAttach() override;
        void onDetach() override;
        void onUi( float p_dt ) override;

    private:
        void drawMenuBar();
        void drawDockspace();
        void syncSceneTexture();
        void syncRenderExtent();
        void saveScene();
        void loadScene();

        TSceneEditorContext m_ctx;
        void*               m_registeredTex = nullptr;
        uint32_t            m_texWidth      = 0;
        uint32_t            m_texHeight     = 0;
        uint32_t            m_texGeneration = 0;
        uint32_t            m_pendingWidth  = 0;
        uint32_t            m_pendingHeight = 0;
        int                 m_pendingStable = 0;
        // After requestRenderExtent, skip rebinding until startFrame applies it.
        uint32_t m_awaitExtentW = 0;
        uint32_t m_awaitExtentH = 0;
        bool     m_layoutBuilt  = false;
    };
}  // namespace Tomos
