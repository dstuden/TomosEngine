#include "Tomos/ui/editor/TSceneEditorLayer.hh"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <imgui.h>
#include <imgui_internal.h>

#include "Tomos/core/app/TApplication.hh"
#include "Tomos/gpu/vulkan/TVkGpu.hh"
#include "Tomos/ui/TImGuiBackend.hh"
#include "Tomos/ui/editor/TAssetBrowserPanel.hh"
#include "Tomos/ui/editor/TConsolePanel.hh"
#include "Tomos/ui/editor/TPhysicsDebugPanel.hh"
#include "Tomos/ui/editor/TRendererDebugPanel.hh"
#include "Tomos/ui/editor/TSceneHierarchyPanel.hh"
#include "Tomos/ui/editor/TSceneInspectorPanel.hh"
#include "Tomos/ui/editor/TSceneViewportPanel.hh"
#include "Tomos/core/scene/TSceneSerializer.hh"

namespace Tomos
{
    TSceneEditorLayer::TSceneEditorLayer( TScene& p_scene, std::string p_scenePath ) : TUiLayer( std::make_unique<TImGuiBackend>(), "SceneEditor" )
    {
        m_ctx.m_scene     = &p_scene;
        m_ctx.m_bag       = &p_scene.resources();
        m_ctx.m_scenePath = std::move( p_scenePath );
        m_ctx.m_playing   = true;
        m_ctx.syncSimulationFlag();
    }

    TSceneEditorLayer::~TSceneEditorLayer() = default;

    void TSceneEditorLayer::onAttach()
    {
        TUiLayer::onAttach();
        if ( auto* gpu = TApplication::get().gpu() ) gpu->setPresentMode( TVkGpu::TPresentMode::EditorViewport );
    }

    void TSceneEditorLayer::onDetach()
    {
        auto* gpu = TApplication::get().gpu();
        // Single device idle for texture retire + ImGui shutdown + present-mode flip.
        if ( gpu ) gpu->waitIdle();

        if ( m_registeredTex != nullptr )
        {
            TImGuiBackend::unregisterTexture( m_registeredTex );
            m_registeredTex      = nullptr;
            m_ctx.m_sceneTexture = 0;
            m_texGeneration      = 0;
        }
        m_awaitExtentW = 0;
        m_awaitExtentH = 0;

        // Skip TUiLayer::onDetach — it would waitIdle again.
        backend().shutdown();

        if ( gpu ) gpu->setPresentMode( TVkGpu::TPresentMode::Swapchain, false );
        if ( m_ctx.m_scene != nullptr ) m_ctx.m_scene->setSimulationPlaying( true );
    }

    void TSceneEditorLayer::saveScene()
    {
        if ( m_ctx.m_scene == nullptr || m_ctx.m_bag == nullptr )
        {
            m_ctx.m_status = "Save failed: no scene";
            return;
        }
        auto& app = TApplication::get();
        if ( TSceneSerializer::saveToFile( *m_ctx.m_scene, app.assetSystem(), m_ctx.m_scenePath ) )
            m_ctx.m_status = "Saved " + m_ctx.m_scenePath;
        else
            m_ctx.m_status = "Save failed: " + m_ctx.m_scenePath;
    }

    void TSceneEditorLayer::loadScene()
    {
        if ( m_ctx.m_scene == nullptr )
        {
            m_ctx.m_status = "Load failed: no scene";
            return;
        }
        auto& app = TApplication::get();
        auto* gpu = app.gpu();
        if ( gpu == nullptr )
        {
            m_ctx.m_status = "Load failed: no GPU";
            return;
        }
        m_ctx.clearSelection();
        if ( TSceneSerializer::loadFromFile( *m_ctx.m_scene, app.assetSystem(), *gpu, m_ctx.m_scenePath ) )
        {
            m_ctx.m_status = "Loaded " + m_ctx.m_scenePath;
            if ( m_ctx.m_afterLoad ) m_ctx.m_afterLoad();
        }
        else
            m_ctx.m_status = "Load failed: " + m_ctx.m_scenePath;
    }

    void TSceneEditorLayer::drawMenuBar()
    {
        if ( ImGui::BeginMainMenuBar() )
        {
            if ( ImGui::BeginMenu( "Scene" ) )
            {
                if ( ImGui::MenuItem( "Save" ) ) saveScene();
                if ( ImGui::MenuItem( "Load" ) ) loadScene();
                ImGui::Separator();
                char pathBuf[ 512 ];
                std::snprintf( pathBuf, sizeof( pathBuf ), "%s", m_ctx.m_scenePath.c_str() );
                if ( ImGui::InputText( "Path", pathBuf, sizeof( pathBuf ) ) ) m_ctx.m_scenePath = pathBuf;
                ImGui::EndMenu();
            }
            if ( ImGui::BeginMenu( "View" ) )
            {
                ImGui::MenuItem( "Hierarchy", nullptr, &m_ctx.m_showHierarchy );
                ImGui::MenuItem( "Inspector", nullptr, &m_ctx.m_showInspector );
                ImGui::MenuItem( "Assets", nullptr, &m_ctx.m_showAssets );
                ImGui::MenuItem( "Scene", nullptr, &m_ctx.m_showViewport );
                ImGui::MenuItem( "Renderer", nullptr, &m_ctx.m_showRenderer );
                ImGui::MenuItem( "Physics", nullptr, &m_ctx.m_showPhysics );
                ImGui::MenuItem( "Console", nullptr, &m_ctx.m_showConsole );
                ImGui::EndMenu();
            }

            ImGui::Separator();
            if ( ImGui::Button( m_ctx.m_playing ? "Pause" : "Play" ) )
            {
                m_ctx.m_playing = !m_ctx.m_playing;
                m_ctx.syncSimulationFlag();
            }
            ImGui::SameLine();
            ImGui::TextDisabled( m_ctx.m_playing ? "Simulating" : "Paused" );

            if ( !m_ctx.m_status.empty() )
            {
                ImGui::Separator();
                ImGui::TextDisabled( "%s", m_ctx.m_status.c_str() );
            }
            ImGui::EndMainMenuBar();
        }
    }

    void TSceneEditorLayer::drawDockspace()
    {
        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos( viewport->WorkPos );
        ImGui::SetNextWindowSize( viewport->WorkSize );
        ImGui::SetNextWindowViewport( viewport->ID );

        ImGuiWindowFlags hostFlags = ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
                                     ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus |
                                     ImGuiWindowFlags_NoBackground;

        ImGui::PushStyleVar( ImGuiStyleVar_WindowRounding, 0.0f );
        ImGui::PushStyleVar( ImGuiStyleVar_WindowBorderSize, 0.0f );
        ImGui::PushStyleVar( ImGuiStyleVar_WindowPadding, ImVec2( 0.0f, 0.0f ) );
        ImGui::Begin( "TomosDockHost", nullptr, hostFlags );
        ImGui::PopStyleVar( 3 );

        const ImGuiID dockspaceId = ImGui::GetID( "TomosDockspace" );
        ImGui::DockSpace( dockspaceId, ImVec2( 0.0f, 0.0f ), ImGuiDockNodeFlags_PassthruCentralNode );

        if ( !m_layoutBuilt )
        {
            m_layoutBuilt = true;
            ImGui::DockBuilderRemoveNode( dockspaceId );
            ImGui::DockBuilderAddNode( dockspaceId, ImGuiDockNodeFlags_DockSpace );
            ImGui::DockBuilderSetNodeSize( dockspaceId, viewport->WorkSize );

            ImGuiID dockMain = dockspaceId;
            ImGuiID dockLeft = 0, dockRight = 0, dockBottom = 0;
            ImGui::DockBuilderSplitNode( dockMain, ImGuiDir_Left, 0.22f, &dockLeft, &dockMain );
            ImGui::DockBuilderSplitNode( dockMain, ImGuiDir_Right, 0.28f, &dockRight, &dockMain );
            ImGui::DockBuilderSplitNode( dockMain, ImGuiDir_Down, 0.30f, &dockBottom, &dockMain );

            ImGui::DockBuilderDockWindow( "Hierarchy", dockLeft );
            ImGui::DockBuilderDockWindow( "Assets", dockLeft );
            ImGui::DockBuilderDockWindow( "Inspector", dockRight );
            ImGui::DockBuilderDockWindow( "Scene", dockMain );
            ImGui::DockBuilderDockWindow( "Renderer", dockBottom );
            ImGui::DockBuilderDockWindow( "Physics", dockBottom );
            ImGui::DockBuilderDockWindow( "Console", dockBottom );
            ImGui::DockBuilderFinish( dockspaceId );
        }

        ImGui::End();
    }

    void TSceneEditorLayer::syncRenderExtent()
    {
        auto* gpu = TApplication::get().gpu();
        if ( gpu == nullptr ) return;
        if ( m_ctx.m_desiredWidth == 0 || m_ctx.m_desiredHeight == 0 ) return;

        const uint32_t   wantW = std::max( 1u, m_ctx.m_desiredWidth );
        const uint32_t   wantH = std::max( 1u, m_ctx.m_desiredHeight );
        const VkExtent2D cur   = gpu->renderExtent();

        // Ignore tiny jitter while dragging dock splitters.
        const bool sameAsCurrent = ( std::abs( static_cast<int>( wantW ) - static_cast<int>( cur.width ) ) < 4 &&
                                     std::abs( static_cast<int>( wantH ) - static_cast<int>( cur.height ) ) < 4 );
        if ( sameAsCurrent )
        {
            m_pendingWidth  = 0;
            m_pendingHeight = 0;
            m_pendingStable = 0;
            return;
        }

        // Require the requested size to hold for a couple of frames before paying
        // for waitIdle + HDR/scene recreate (dock layout settles noisily).
        if ( wantW == m_pendingWidth && wantH == m_pendingHeight )
            ++m_pendingStable;
        else
        {
            m_pendingWidth  = wantW;
            m_pendingHeight = wantH;
            m_pendingStable = 1;
        }
        if ( m_pendingStable < 2 ) return;

        // Queue only — startFrame() applies after waitIdle. Keep the ImGui
        // descriptor set alive until then (other in-flight frames may still
        // sample it). Blank the UI handle so this frame does not prefer a
        // stale size; syncSceneTexture rebinds after the flush.
        gpu->requestRenderExtent( wantW, wantH );
        m_awaitExtentW       = wantW;
        m_awaitExtentH       = wantH;
        m_ctx.m_sceneTexture = 0;
        m_pendingWidth       = 0;
        m_pendingHeight      = 0;
        m_pendingStable      = 0;
    }

    void TSceneEditorLayer::syncSceneTexture()
    {
        auto* gpu = TApplication::get().gpu();
        if ( gpu == nullptr || !gpu->sceneColorReady() ) return;

        const VkExtent2D ext = gpu->renderExtent();
        if ( m_awaitExtentW != 0 && ( ext.width != m_awaitExtentW || ext.height != m_awaitExtentH ) ) return;
        m_awaitExtentW = 0;
        m_awaitExtentH = 0;

        const uint32_t    gen  = gpu->sceneColorGeneration();
        const VkImageView view = gpu->sceneColorView();
        const VkSampler   samp = gpu->sceneColorSampler();
        if ( view == VK_NULL_HANDLE || samp == VK_NULL_HANDLE ) return;

        // Rebind whenever the GPU image was recreated (generation), not only on size change.
        if ( m_registeredTex != nullptr && m_texWidth == ext.width && m_texHeight == ext.height && m_texGeneration == gen )
        {
            m_ctx.m_sceneTexture = reinterpret_cast<ImTextureID>( m_registeredTex );
            return;
        }

        // New view is already idle (created in the previous startFrame flush).
        if ( m_registeredTex != nullptr )
        {
            TImGuiBackend::unregisterTexture( m_registeredTex );
            m_registeredTex = nullptr;
        }

        m_registeredTex      = TImGuiBackend::registerTexture( samp, view );
        m_texWidth           = ext.width;
        m_texHeight          = ext.height;
        m_texGeneration      = gen;
        m_ctx.m_sceneTexture = reinterpret_cast<ImTextureID>( m_registeredTex );
    }

    void TSceneEditorLayer::onUi( float p_dt )
    {
        TImGuiBackend::assertWithinUiFrame();
        m_ctx.syncSimulationFlag();

        drawMenuBar();
        drawDockspace();

        syncRenderExtent();
        syncSceneTexture();

        if ( m_ctx.m_showHierarchy ) TSceneHierarchyPanel::draw( m_ctx );
        if ( m_ctx.m_showInspector ) TSceneInspectorPanel::draw( m_ctx );
        if ( m_ctx.m_showAssets ) TAssetBrowserPanel::draw( m_ctx );
        if ( m_ctx.m_showViewport ) TSceneViewportPanel::draw( m_ctx );
        if ( m_ctx.m_showRenderer ) TRendererDebugPanel::draw( m_ctx, p_dt );
        if ( m_ctx.m_showPhysics ) TPhysicsDebugPanel::draw( m_ctx );
        if ( m_ctx.m_showConsole ) TConsolePanel::draw( m_ctx );
    }
}  // namespace Tomos
