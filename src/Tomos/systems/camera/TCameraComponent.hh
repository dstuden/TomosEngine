#pragma once

#include <glm/glm.hpp>

#include "../TComponent.hh"

namespace Tomos
{
    class TCameraComponent : public TComponent
    {
    public:
        explicit TCameraComponent( float p_fov = 45.0f, float p_near = 0.1f, float p_far = 100.0f, const std::string& p_name = "Camera" );

        void setFov( float p_fov );
        void setNear( float p_near );
        void setFar( float p_far );

        float getFov() const;
        float getNear() const;
        float getFar() const;

        glm::mat4 getProjection();
        glm::mat4 getInvProjection();

        bool m_active = true;

    protected:
        void recompute();

        glm::mat4 m_projection    = glm::mat4( 1.0f );
        glm::mat4 m_invProjection = glm::mat4( 1.0f );
        bool      m_dirty{};
        float     m_fov{};
        float     m_far{};
        float     m_near{};
        float     m_aspectRatio = 0.0f;
    };
} // namespace Tomos
