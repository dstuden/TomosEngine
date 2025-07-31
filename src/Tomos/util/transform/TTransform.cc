//
// Created by dstuden on 1/20/25.
//
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/string_cast.hpp>

#include "../logger/TLogger.hh"
#include "TTransform.hh"

namespace Tomos
{
    TTransform::TTransform( const glm::vec3& p_translation, const glm::quat& p_rotation, const glm::vec3& p_scale ) :
        m_translation( p_translation ), m_rotation( p_rotation ), m_scale( p_scale ), m_dirty( false )
    {
        m_localMat    = glm::mat4( 1.0f );
        m_localInvMat = glm::mat4( 1.0f );
        m_globMat     = glm::mat4( 1.0f );
        m_globInvMat  = glm::mat4( 1.0f );
        updateLocal();
    }

    void TTransform::update() { m_dirty = true; }

    void TTransform::updateLocal()
    {
        m_localMat = glm::mat4( 1.0f );
        m_localMat = glm::translate( m_localMat, m_translation );
        m_localMat *= glm::mat4_cast( m_rotation ); // Convert quaternion to matrix and multiply
        m_localMat = glm::scale( m_localMat, m_scale );

        m_localInvMat = glm::inverse( m_localMat );
        m_dirty       = false;
    }

    void TTransform::updateGlobal( const TTransform& p_parent )
    {
        if ( m_dirty )
        {
            updateLocal();
        }

        m_globMat    = p_parent.m_globMat * m_localMat;
        m_globInvMat = m_localInvMat * p_parent.m_globInvMat;
    }

    const glm::mat4& TTransform::getLocalMatrix()
    {
        if ( m_dirty )
        {
            updateLocal();
        }
        return m_localMat;
    }

    const glm::mat4& TTransform::getLocalInvMatrix()
    {
        if ( m_dirty )
        {
            updateLocal();
        }
        return m_localInvMat;
    }
} // namespace Tomos
