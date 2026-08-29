#include "Tomos/ui/editor/TRendererDebugPanel.hh"

#include <imgui.h>

#include "Tomos/core/app/TApplication.hh"
#include "Tomos/gpu/vulkan/TVkGpu.hh"
#include "Tomos/gpu/vulkan/TVkPass.hh"
#include "Tomos/gpu/vulkan/post/TPostBloom.hh"
#include "Tomos/gpu/vulkan/post/TPostFog.hh"
#include "Tomos/gpu/vulkan/post/TPostSAO.hh"
#include "Tomos/gpu/vulkan/post/TPostTonemap.hh"
#include "Tomos/gpu/vulkan/renderer/TVkClusteredRenderer.hh"

namespace Tomos
{
    void TRendererDebugPanel::draw( TSceneEditorContext& /*p_ctx*/, float p_dt )
    {
        auto* gpu = TApplication::get().gpu();
        if ( gpu == nullptr ) return;

        TFrameState& state = gpu->frameState();

        ImGui::Begin( "Renderer" );

        static const char* kDebugViews[] = { "Off", "Cluster grid", "Light heatmap", "Depth slices" };
        int                mode          = static_cast<int>( state.m_debugMode );
        if ( ImGui::Combo( "Debug view", &mode, kDebugViews, 4 ) ) state.m_debugMode = static_cast<TDebugView>( mode );

        static float sSmoothDt = p_dt;
        sSmoothDt              = sSmoothDt * 0.9f + p_dt * 0.1f;
        ImGui::Text( "Frame: %.2f ms (%.0f fps)", sSmoothDt * 1000.0f, sSmoothDt > 0.0f ? 1.0f / sSmoothDt : 0.0f );

        const VkExtent2D re = gpu->renderExtent();
        const VkExtent2D se = gpu->extent();
        ImGui::Text( "Render: %ux%u   Swap: %ux%u", re.width, re.height, se.width, se.height );

        {
            auto& app = TApplication::get();

            static const char* kWindowModes[] = { "Windowed", "Borderless fullscreen", "Exclusive fullscreen" };
            int                winMode        = static_cast<int>( app.window().windowMode() );
            if ( ImGui::Combo( "Window mode", &winMode, kWindowModes, 3 ) )
            {
                const TWindowMode mode    = static_cast<TWindowMode>( winMode );
                const TWindowMode prev    = app.window().windowMode();
                if ( prev == TWindowMode::Windowed && mode != TWindowMode::Windowed )
                {
                    app.config().m_windowWidth  = static_cast<unsigned int>( app.window().getData().m_width );
                    app.config().m_windowHeight = static_cast<unsigned int>( app.window().getData().m_height );
                }
                app.window().setWindowMode( mode );
                app.config().m_windowMode = TWindow::windowModeToString( mode );
                if ( mode == TWindowMode::Windowed )
                {
                    app.config().m_windowWidth  = static_cast<unsigned int>( app.window().windowedWidth() );
                    app.config().m_windowHeight = static_cast<unsigned int>( app.window().windowedHeight() );
                }
                app.configManager().save();
            }

            static const char* kPresentPolicies[] = { "FIFO (vsync)", "Mailbox", "Immediate" };
            int                policyIdx          = static_cast<int>( gpu->swapchainPresentPolicy() );
            if ( ImGui::Combo( "Present mode", &policyIdx, kPresentPolicies, 3 ) )
            {
                const auto policy = static_cast<TVkGpu::TSwapchainPresentPolicy>( policyIdx );
                gpu->setSwapchainPresentPolicy( policy );
                app.config().m_presentMode = TVkGpu::presentPolicyToString( policy );
                app.configManager().save();
            }
            ImGui::Text( "Active VkPresentMode: %s", TVkGpu::vkPresentModeName( gpu->activePresentMode() ) );
        }

        ImGui::Separator();

        size_t forwardDraws = 0, shadowOnly = 0;
        for ( const auto& dc : state.m_drawCalls )
        {
            if ( dc.m_visible )
                ++forwardDraws;
            else
                ++shadowOnly;
        }
        ImGui::Text( "Draw calls: %zu forward, %zu shadow-only", forwardDraws, shadowOnly );
        ImGui::Text( "Instances:  %zu", state.m_instances.size() );
        ImGui::Text( "Meshes:     %u total, %u frustum-culled", state.m_meshesTotal, state.m_meshesCulled );
        ImGui::Text( "Lights:     %zu", state.m_lights.size() );
        ImGui::Text( "Sprites:    %zu (%zu batches)", state.m_sprites.size(), state.m_spriteBatches.size() );
        ImGui::Text( "Emitters:   %zu (GPU pool %u)", state.m_emitters.size(), g_kMaxParticles );
        ImGui::Text( "Bones:      %zu uploaded", state.m_bones.size() );

        int shadowCasters = 0;
        for ( const auto& l : state.m_lights )
            if ( l.m_shadowMap >= 0 ) ++shadowCasters;
        ImGui::Text( "Shadow maps: %d / %u", shadowCasters, g_kMaxShadowMaps );

        if ( ImGui::CollapsingHeader( "Lights" ) )
        {
            if ( ImGui::BeginTable( "lights", 5, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY, ImVec2( 0, 140 ) ) )
            {
                ImGui::TableSetupColumn( "#" );
                ImGui::TableSetupColumn( "Type" );
                ImGui::TableSetupColumn( "Intensity" );
                ImGui::TableSetupColumn( "Range" );
                ImGui::TableSetupColumn( "Shadow" );
                ImGui::TableHeadersRow();
                for ( size_t i = 0; i < state.m_lights.size(); ++i )
                {
                    const auto& l = state.m_lights[ i ];
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::Text( "%zu", i );
                    ImGui::TableNextColumn();
                    const int t = static_cast<int>( l.m_type );
                    ImGui::TextUnformatted( t == 0 ? "Point" : ( t == 1 ? "Dir" : "Spot" ) );
                    ImGui::TableNextColumn();
                    ImGui::Text( "%.2f", l.m_intensity );
                    ImGui::TableNextColumn();
                    ImGui::Text( "%.1f", l.m_maxRange );
                    ImGui::TableNextColumn();
                    ImGui::Text( "%d", l.m_shadowMap );
                }
                ImGui::EndTable();
            }
        }

        if ( ImGui::CollapsingHeader( "Draw calls" ) )
        {
            if ( ImGui::BeginTable( "draws", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY, ImVec2( 0, 140 ) ) )
            {
                ImGui::TableSetupColumn( "#" );
                ImGui::TableSetupColumn( "Instances" );
                ImGui::TableSetupColumn( "Visible" );
                ImGui::TableHeadersRow();
                for ( size_t i = 0; i < state.m_drawCalls.size() && i < 64; ++i )
                {
                    const auto& dc = state.m_drawCalls[ i ];
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::Text( "%zu", i );
                    ImGui::TableNextColumn();
                    ImGui::Text( "%u", dc.m_instanceCount );
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted( dc.m_visible ? "yes" : "shadow" );
                }
                ImGui::EndTable();
            }
        }

        if ( auto* renderer = gpu->renderer() )
        {
            if ( ImGui::CollapsingHeader( "Post effects", ImGuiTreeNodeFlags_DefaultOpen ) )
            {
                for ( auto& effect : renderer->postStack().effects() )
                {
                    if ( dynamic_cast<TPostTonemap*>( effect.get() ) ) continue;
                    ImGui::Checkbox( effect->name(), &effect->m_enabled );
                }
                if ( auto* fog = renderer->postStack().find<TPostFog>() )
                {
                    ImGui::SliderFloat( "Fog density", &fog->m_density, 0.0f, 0.1f, "%.4f" );
                    ImGui::ColorEdit3( "Fog color", &fog->m_color.x );
                }
                if ( auto* bloom = renderer->postStack().find<TPostBloom>() )
                {
                    ImGui::SliderFloat( "Bloom threshold", &bloom->m_threshold, 0.0f, 4.0f );
                    ImGui::SliderFloat( "Bloom strength", &bloom->m_strength, 0.0f, 2.0f );
                }
                if ( auto* sao = renderer->postStack().find<TPostSAO>() )
                {
                    ImGui::SliderFloat( "SAO radius", &sao->m_radius, 0.05f, 2.0f, "%.2f" );
                    ImGui::SliderFloat( "SAO bias", &sao->m_bias, 0.001f, 0.2f, "%.3f" );
                    ImGui::SliderFloat( "SAO intensity", &sao->m_intensity, 0.0f, 2.0f );
                    ImGui::SliderFloat( "SAO blur sharpness", &sao->m_blurSharpness, 10.0f, 500.0f, "%.0f" );
                    ImGui::SliderFloat( "SAO temporal", &sao->m_temporalBlend, 0.0f, 0.9f, "%.2f" );
                }
            }
        }

        if ( ImGui::CollapsingHeader( "Camera" ) )
        {
            const glm::vec3 pos = glm::vec3( state.m_viewInv[ 3 ] );
            ImGui::Text( "Position: %.2f  %.2f  %.2f", pos.x, pos.y, pos.z );
            ImGui::Text( "Near/Far: %.2f / %.1f", state.m_near, state.m_far );
        }

        ImGui::End();
    }
}  // namespace Tomos
