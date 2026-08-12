#pragma once

#include <glm/glm.hpp>
#include <imgui.h>

#include "Tomos/util/math/TFrustum.hh"

namespace Tomos
{
    // Project world-space debug primitives into an ImGui Scene panel rect.
    class TDebugDraw
    {
    public:
        static bool project( const glm::mat4& p_viewProj, const ImVec2& p_panelPos, const ImVec2& p_panelSize, const glm::vec3& p_world, ImVec2& p_out );

        static void aabb( ImDrawList* p_dl, const glm::mat4& p_viewProj, const ImVec2& p_panelPos, const ImVec2& p_panelSize, const TAABB& p_box, ImU32 p_col,
                          float p_thickness = 1.5f );

        static void line( ImDrawList* p_dl, const glm::mat4& p_viewProj, const ImVec2& p_panelPos, const ImVec2& p_panelSize, const glm::vec3& p_a,
                          const glm::vec3& p_b, ImU32 p_col, float p_thickness = 1.5f );
    };
}  // namespace Tomos
