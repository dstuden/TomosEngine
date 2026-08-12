#include "Tomos/ui/editor/TRendererDebugPanel.hh"

#include <imgui.h>

#include "Tomos/core/app/TApplication.hh"
#include "Tomos/gpu/vulkan/TVkGpu.hh"
#include "Tomos/gpu/vulkan/TVkPass.hh"
#include "Tomos/gpu/vulkan/post/TPostBloom.hh"
#include "Tomos/gpu/vulkan/post/TPostFog.hh"
#include "Tomos/gpu/vulkan/post/TPostSSAO.hh"
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

        static const char* k_debugViews[] = { "Off", "Cluster grid", "Light heatmap", "Depth slices" };
        int                mode           = static_cast<int>( state.m_debugMode );
        if ( ImGui::Combo( "Debug view", &mode, k_debugViews, 4 ) ) state.m_debugMode = static_cast<TDebugView>( mode );

        static float s_smoothDt = p_dt;
        s_smoothDt              = s_smoothDt * 0.9f + p_dt * 0.1f;
        ImGui::Text( "Frame: %.2f ms (%.0f fps)", s_smoothDt * 1000.0f, s_smoothDt > 0.0f ? 1.0f / s_smoothDt : 0.0f );

        const VkExtent2D re = gpu->renderExtent();
        const VkExtent2D se = gpu->extent();
        ImGui::Text( "Render: %ux%u   Swap: %ux%u", re.width, re.height, se.width, se.height );

        {
            auto& app = TApplication::get();
            bool  fs  = app.window().isFullscreen();
            if ( ImGui::Checkbox( "Fullscreen", &fs ) )
            {
                app.window().setFullscreen( fs );
                app.config().fullscreen = fs;
                if ( !fs )
                {
                    app.config().windowWidth  = static_cast<unsigned int>( app.window().getData().m_width );
                    app.config().windowHeight = static_cast<unsigned int>( app.window().getData().m_height );
                }
                app.configManager().save();
            }
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
        ImGui::Text( "Emitters:   %zu (GPU pool %u)", state.m_emitters.size(), k_maxParticles );
        ImGui::Text( "Bones:      %zu uploaded", state.m_bones.size() );

        int shadowCasters = 0;
        for ( const auto& L : state.m_lights )
            if ( L.m_shadowMap >= 0 ) ++shadowCasters;
        ImGui::Text( "Shadow maps: %d / %u", shadowCasters, k_maxShadowMaps );

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
                    const auto& L = state.m_lights[ i ];
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::Text( "%zu", i );
                    ImGui::TableNextColumn();
                    const int t = static_cast<int>( L.m_type );
                    ImGui::TextUnformatted( t == 0 ? "Point" : ( t == 1 ? "Dir" : "Spot" ) );
                    ImGui::TableNextColumn();
                    ImGui::Text( "%.2f", L.m_intensity );
                    ImGui::TableNextColumn();
                    ImGui::Text( "%.1f", L.m_maxRange );
                    ImGui::TableNextColumn();
                    ImGui::Text( "%d", L.m_shadowMap );
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
                if ( auto* ssao = renderer->postStack().find<TPostSSAO>() )
                {
                    ImGui::SliderFloat( "SSAO radius", &ssao->m_radius, 0.05f, 2.0f, "%.2f" );
                    ImGui::SliderFloat( "SSAO bias", &ssao->m_bias, 0.001f, 0.2f, "%.3f" );
                    ImGui::SliderFloat( "SSAO intensity", &ssao->m_intensity, 0.0f, 2.0f );
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
