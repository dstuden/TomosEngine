#pragma once

#include <cstdint>
#include <functional>
#include <imgui.h>
#include <string>

#include "Tomos/core/app/TApplication.hh"
#include "Tomos/core/scene/TScene.hh"
#include "Tomos/core/scene/TSceneResourceBag.hh"

namespace Tomos
{
    enum class TGizmoMode : int
    {
        Translate = 0,
        Rotate    = 1,
        Scale     = 2,
    };

    struct TSceneEditorContext
    {
        TScene*            m_scene      = nullptr;
        TSceneResourceBag* m_bag        = nullptr;
        uint64_t           m_selectedId = 0;
        std::string        m_scenePath;
        std::string        m_status;

        bool m_showHierarchy = true;
        bool m_showInspector = true;
        bool m_showViewport  = true;
        bool m_showAssets    = true;
        bool m_showRenderer  = true;
        bool m_showPhysics   = true;
        bool m_showConsole   = true;

        bool m_playing = true;

        ImTextureID m_sceneTexture = 0;
        ImVec2      m_viewportPos{ 0.0f, 0.0f };
        ImVec2      m_viewportSize{ 0.0f, 0.0f };
        uint32_t    m_desiredWidth    = 0;
        uint32_t    m_desiredHeight   = 0;
        bool        m_viewportHovered = false;
        bool        m_viewportFocused = false;

        TGizmoMode m_gizmoMode        = TGizmoMode::Translate;
        bool       m_gizmoLocal       = true;
        bool       m_overlaySelection = true;
        bool       m_overlayColliders = true;
        bool       m_overlayVelocity  = false;

        std::function<void()> m_afterLoad;

        [[nodiscard]] TSceneNode* selectedNode() const
        {
            if ( m_scene == nullptr || m_selectedId == 0 ) return nullptr;
            return m_scene->findById( m_selectedId );
        }

        void select( uint64_t p_id ) { m_selectedId = p_id; }
        void clearSelection() { m_selectedId = 0; }

        void syncSimulationFlag() const { TApplication::get().sceneManager().setSimulationPlaying( m_playing ); }
    };
}  // namespace Tomos
