#include "Tomos/ui/editor/TConsolePanel.hh"

#include <imgui.h>

#include "Tomos/util/logger/TLogger.hh"

namespace Tomos
{
    void TConsolePanel::draw( TSceneEditorContext& /*p_ctx*/ )
    {
        ImGui::Begin( "Console" );

        static bool showInfo   = true;
        static bool showDebug  = true;
        static bool showWarn   = true;
        static bool showError  = true;
        static bool autoScroll = true;

        ImGui::Checkbox( "Info", &showInfo );
        ImGui::SameLine();
        ImGui::Checkbox( "Debug", &showDebug );
        ImGui::SameLine();
        ImGui::Checkbox( "Warn", &showWarn );
        ImGui::SameLine();
        ImGui::Checkbox( "Error", &showError );
        ImGui::SameLine();
        ImGui::Checkbox( "Auto-scroll", &autoScroll );
        ImGui::SameLine();
        if ( ImGui::Button( "Clear" ) ) TLogger::clearRing();

        ImGui::Separator();
        ImGui::BeginChild( "LogScroll", ImVec2( 0, 0 ), false, ImGuiWindowFlags_HorizontalScrollbar );

        const auto entries = TLogger::snapshotRing();
        for ( const auto& e : entries )
        {
            if ( e.m_level == TLogLevel::INFO && !showInfo ) continue;
            if ( e.m_level == TLogLevel::DEBUG && !showDebug ) continue;
            if ( e.m_level == TLogLevel::WARN && !showWarn ) continue;
            if ( e.m_level == TLogLevel::ERROR && !showError ) continue;

            ImVec4      col( 0.8f, 0.8f, 0.8f, 1.0f );
            const char* tag = "INFO";
            switch ( e.m_level )
            {
                case TLogLevel::DEBUG:
                    col = ImVec4( 0.4f, 0.9f, 0.4f, 1.0f );
                    tag = "DEBUG";
                    break;
                case TLogLevel::WARN:
                    col = ImVec4( 0.95f, 0.8f, 0.3f, 1.0f );
                    tag = "WARN";
                    break;
                case TLogLevel::ERROR:
                    col = ImVec4( 1.0f, 0.4f, 0.4f, 1.0f );
                    tag = "ERROR";
                    break;
                default:
                    break;
            }
            ImGui::PushStyleColor( ImGuiCol_Text, col );
            ImGui::TextUnformatted( ( e.m_time + "[" + tag + "] " + e.m_function + ": " + e.m_message ).c_str() );
            ImGui::PopStyleColor();
        }

        if ( autoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 4.0f ) ImGui::SetScrollHereY( 1.0f );

        ImGui::EndChild();
        ImGui::End();
    }
}  // namespace Tomos
