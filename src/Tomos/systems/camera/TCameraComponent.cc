//
// Created by dstuden on 1/20/25.
//

#include "TCameraComponent.hh"
#include "Tomos/core/TApplication.hh"

namespace Tomos
{
    TCameraComponent::TCameraComponent( float p_fov, float p_near, float p_far, const std::string& p_name ) :
        m_fov( p_fov ), m_near( p_near ), m_far( p_far ), m_dirty( true )
    {
        m_name = p_name;
        recompute();
    }

    void TCameraComponent::setFov( float p_fov )
    {
        if ( m_fov != p_fov )
        {
            m_fov   = p_fov;
            m_dirty = true;
        }
    }

    void TCameraComponent::setNear( float p_near )
    {
        if ( m_near != p_near )
        {
            m_near  = p_near;
            m_dirty = true;
        }
    }

    void TCameraComponent::setFar( float p_far )
    {
        if ( m_far != p_far )
        {
            m_far   = p_far;
            m_dirty = true;
        }
    }

    float TCameraComponent::getFov() const { return m_fov; }

    float TCameraComponent::getNear() const { return m_near; }

    float TCameraComponent::getFar() const { return m_far; }

    void TCameraComponent::recompute()
    {
        m_projection    = glm::perspective( glm::radians( m_fov ), TApplication::get()->getWindow().getData().m_aspectRatio, m_near, m_far );
        m_aspectRatio   = TApplication::get()->getWindow().getData().m_aspectRatio;
        m_invProjection = glm::inverse( m_projection );
        m_dirty         = false;
    }

    glm::mat4 TCameraComponent::getProjection()
    {
        if ( m_dirty || TApplication::get()->getWindow().getData().m_aspectRatio != m_aspectRatio )
        {
            recompute();
        }
        return m_projection;
    }

    glm::mat4 TCameraComponent::getInvProjection()
    {
        if ( m_dirty || TApplication::get()->getWindow().getData().m_aspectRatio != m_aspectRatio )
        {
            recompute();
        }
        return m_invProjection;
    }
}  // namespace Tomos
