#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace Tomos
{
    // GLM projections use OpenGL NDC (Y up). Vulkan NDC is Y down — flip after
    // building so cameras and shadow casters share the same clip space.
    // Depth stays 0–1 via GLM_FORCE_DEPTH_ZERO_TO_ONE.
    inline glm::mat4 perspectiveVk( float p_fovyRadians, float p_aspect, float p_near, float p_far )
    {
        glm::mat4 m = glm::perspective( p_fovyRadians, p_aspect, p_near, p_far );
        m[ 1 ][ 1 ] *= -1.0f;
        return m;
    }

    inline glm::mat4 orthoVk( float p_left, float p_right, float p_bottom, float p_top, float p_near, float p_far )
    {
        glm::mat4 m = glm::ortho( p_left, p_right, p_bottom, p_top, p_near, p_far );
        m[ 1 ][ 1 ] *= -1.0f;
        return m;
    }
}  // namespace Tomos
