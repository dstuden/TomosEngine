#include "Tomos/ui/editor/TDebugDraw.hh"

namespace Tomos
{
    bool TDebugDraw::project( const glm::mat4& p_viewProj, const ImVec2& p_panelPos, const ImVec2& p_panelSize, const glm::vec3& p_world, ImVec2& p_out )
    {
        const glm::vec4 clip = p_viewProj * glm::vec4( p_world, 1.0f );
        if ( clip.w <= 1e-5f ) return false;

        const glm::vec3 ndc = glm::vec3( clip ) / clip.w;
        // Vulkan Y is already flipped in the camera proj → same Y-down as ImGui.
        p_out.x = p_panelPos.x + ( ndc.x * 0.5f + 0.5f ) * p_panelSize.x;
        p_out.y = p_panelPos.y + ( ndc.y * 0.5f + 0.5f ) * p_panelSize.y;
        return true;
    }

    void TDebugDraw::aabb( ImDrawList* p_dl, const glm::mat4& p_viewProj, const ImVec2& p_panelPos, const ImVec2& p_panelSize, const TAABB& p_box, ImU32 p_col,
                           float p_thickness )
    {
        if ( p_dl == nullptr || !p_box.valid() ) return;

        const std::array<glm::vec3, 8> corners = p_box.corners();

        ImVec2 screen[ 8 ];
        bool   ok[ 8 ];
        for ( int i = 0; i < 8; ++i ) ok[ i ] = project( p_viewProj, p_panelPos, p_panelSize, corners[ static_cast<size_t>( i ) ], screen[ i ] );

        static constexpr int kEdges[ 12 ][ 2 ] = {
                { 0, 1 }, { 1, 3 }, { 3, 2 }, { 2, 0 }, { 4, 5 }, { 5, 7 }, { 7, 6 }, { 6, 4 }, { 0, 4 }, { 1, 5 }, { 2, 6 }, { 3, 7 },
        };

        for ( const auto& e : kEdges )
        {
            if ( !ok[ e[ 0 ] ] || !ok[ e[ 1 ] ] ) continue;
            p_dl->AddLine( screen[ e[ 0 ] ], screen[ e[ 1 ] ], p_col, p_thickness );
        }
    }

    void TDebugDraw::line( ImDrawList* p_dl, const glm::mat4& p_viewProj, const ImVec2& p_panelPos, const ImVec2& p_panelSize, const glm::vec3& p_a,
                           const glm::vec3& p_b, ImU32 p_col, float p_thickness )
    {
        if ( p_dl == nullptr ) return;
        ImVec2 a, b;
        if ( !project( p_viewProj, p_panelPos, p_panelSize, p_a, a ) ) return;
        if ( !project( p_viewProj, p_panelPos, p_panelSize, p_b, b ) ) return;
        p_dl->AddLine( a, b, p_col, p_thickness );
    }
}  // namespace Tomos
