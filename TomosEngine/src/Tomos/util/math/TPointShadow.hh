#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "Tomos/util/math/TProjection.hh"

namespace Tomos
{
    // Point-light shadows use six consecutive layers in the 2D shadow-map array
    // (one 90° face each).  Face order and up vectors must match point_shadow.glsl.
    inline constexpr uint32_t g_kPointShadowFaces = 6;
    inline constexpr float    g_kPointShadowNear  = 0.1f;

    // +X, -X, +Y, -Y, +Z, -Z — same convention as a GLES/Vulkan cubemap.
    inline void pointShadowFaceBasis( uint32_t p_face, glm::vec3& p_target, glm::vec3& p_up )
    {
        switch ( p_face % g_kPointShadowFaces )
        {
            case 0:
                p_target = { 1.0f, 0.0f, 0.0f };
                p_up     = { 0.0f, -1.0f, 0.0f };
                break;
            case 1:
                p_target = { -1.0f, 0.0f, 0.0f };
                p_up     = { 0.0f, -1.0f, 0.0f };
                break;
            case 2:
                p_target = { 0.0f, 1.0f, 0.0f };
                p_up     = { 0.0f, 0.0f, 1.0f };
                break;
            case 3:
                p_target = { 0.0f, -1.0f, 0.0f };
                p_up     = { 0.0f, 0.0f, -1.0f };
                break;
            case 4:
                p_target = { 0.0f, 0.0f, 1.0f };
                p_up     = { 0.0f, -1.0f, 0.0f };
                break;
            default:
                p_target = { 0.0f, 0.0f, -1.0f };
                p_up     = { 0.0f, -1.0f, 0.0f };
                break;
        }
    }

    inline glm::mat4 pointShadowFaceVP( const glm::vec3& p_lightPos, float p_range, uint32_t p_face )
    {
        glm::vec3 target, up;
        pointShadowFaceBasis( p_face, target, up );

        const float     farZ = glm::max( p_range, g_kPointShadowNear + 0.01f );
        const glm::mat4 view = glm::lookAt( p_lightPos, p_lightPos + target, up );
        const glm::mat4 proj = perspectiveVk( glm::half_pi<float>(), 1.0f, g_kPointShadowNear, farZ );
        return proj * view;
    }
}  // namespace Tomos
