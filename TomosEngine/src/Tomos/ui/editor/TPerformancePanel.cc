#include "Tomos/ui/editor/TPerformancePanel.hh"

#include <imgui.h>

#if TOMOS_DEBUG
#include "Tomos/util/profile/TFrameAnalytics.hh"
#include "Tomos/util/profile/TFrameProfiler.hh"
#endif

namespace Tomos
{
    void TPerformancePanel::draw( TSceneEditorContext& p_ctx )
    {
#if !TOMOS_DEBUG
        ( void ) p_ctx;
        ImGui::Begin( "Performance", &p_ctx.m_showPerformance );
        ImGui::TextUnformatted( "Available in Debug builds only." );
        ImGui::End();
#else
        TFrameProfiler& profiler = TFrameProfiler::get();

        if ( !ImGui::Begin( "Performance", &p_ctx.m_showPerformance ) )
        {
            if ( !p_ctx.m_showPerformance )
            {
                p_ctx.m_perfCapture = false;
                profiler.setCaptureEnabled( false );
            }
            ImGui::End();
            return;
        }

        // First open turns Capture on.
        if ( !p_ctx.m_perfCaptureInitialized )
        {
            p_ctx.m_perfCaptureInitialized = true;
            p_ctx.m_perfCapture             = true;
            profiler.setCaptureEnabled( true );
        }

        if ( ImGui::Checkbox( "Capture", &p_ctx.m_perfCapture ) )
            profiler.setCaptureEnabled( p_ctx.m_perfCapture );
        else if ( p_ctx.m_perfCapture != profiler.isCaptureEnabled() )
            profiler.setCaptureEnabled( p_ctx.m_perfCapture );

        ImGui::SameLine();
        ImGui::Checkbox( "Freeze", &p_ctx.m_perfFreeze );

        static TFrameAnalyticsSnapshot sFrozen{};
        static float                   sSmoothRealMs = 16.0f;

        const TFrameAnalyticsSnapshot& live = TFrameAnalytics::get().last();
        if ( !p_ctx.m_perfFreeze )
        {
            sFrozen        = live;
            sSmoothRealMs  = sSmoothRealMs * 0.9f + live.realDtMs * 0.1f;
        }

        const TFrameAnalyticsSnapshot& d         = p_ctx.m_perfFreeze ? sFrozen : live;
        const float                    displayMs = p_ctx.m_perfFreeze ? d.realDtMs : sSmoothRealMs;
        const float                    fps       = displayMs > 0.0f ? 1000.0f / displayMs : 0.0f;

        ImGui::Separator();
        ImGui::Text( "Frame: %.2f ms (%.0f fps)   raw %.2f ms", displayMs, fps, d.realDtMs );
        ImGui::Text( "Sim dt: %.2f ms", d.simDtMs );

        if ( ImGui::CollapsingHeader( "CPU", ImGuiTreeNodeFlags_DefaultOpen ) )
        {
            if ( d.cpu.empty() )
                ImGui::TextDisabled( "No samples (enable Capture)" );
            else
            {
                for ( const auto& s : d.cpu )
                    ImGui::Text( "%*s%s  %.3f ms", s.depth * 2, "", s.name != nullptr ? s.name : "?", s.durationMs );
            }
        }

        if ( ImGui::CollapsingHeader( "GPU", ImGuiTreeNodeFlags_DefaultOpen ) )
        {
            if ( !d.gpuValid )
                ImGui::TextDisabled( "Waiting for timestamp results…" );
            else
            {
                float total = 0.0f;
                for ( const auto& g : d.gpu )
                {
                    ImGui::Text( "%-14s %6.3f ms", g.name != nullptr ? g.name : "?", g.ms );
                    total += g.ms;
                }
                ImGui::Separator();
                ImGui::Text( "%-14s %6.3f ms", "Sum", total );
            }
        }

        if ( ImGui::CollapsingHeader( "Frame memory", ImGuiTreeNodeFlags_DefaultOpen ) )
        {
            ImGui::Text( "Arena:  %llu B / %llu allocs (peak %llu B)", static_cast<unsigned long long>( d.memory.m_arenaBytes ),
                         static_cast<unsigned long long>( d.memory.m_arenaAllocs ), static_cast<unsigned long long>( d.memory.m_arenaPeakBytes ) );
            ImGui::Text( "Heap probes: +%llu B / %llu scopes", static_cast<unsigned long long>( d.memory.m_heapProbeBytes ),
                         static_cast<unsigned long long>( d.memory.m_heapProbeCount ) );
        }

        if ( ImGui::CollapsingHeader( "Scene", ImGuiTreeNodeFlags_DefaultOpen ) )
        {
            ImGui::Text( "Draw calls: %zu forward, %zu shadow-only", d.scene.drawCallsForward, d.scene.drawCallsShadow );
            ImGui::Text( "Instances:  %zu", d.scene.instances );
            ImGui::Text( "Meshes:     %u total, %u frustum-culled", d.scene.meshesTotal, d.scene.meshesCulled );
            ImGui::Text( "Lights:     %zu", d.scene.lights );
            ImGui::Text( "Sprites:    %zu (%zu batches)", d.scene.sprites, d.scene.spriteBatches );
            ImGui::Text( "Emitters:   %zu", d.scene.emitters );
            ImGui::Text( "Bones:      %zu", d.scene.bones );
        }

        ImGui::End();
#endif
    }
}  // namespace Tomos
