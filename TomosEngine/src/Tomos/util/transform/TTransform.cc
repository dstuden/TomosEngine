#include "TTransform.hh"

namespace Tomos
{
    TTransform::TTransform( const glm::vec3& p_translation, const glm::quat& p_rotation, const glm::vec3& p_scale ) :
        m_translation( p_translation ), m_rotation( p_rotation ), m_scale( p_scale ), m_localDirty( true ), m_globalDirty( true )
    {
    }

    void TTransform::updateLocal() const
    {
        m_localMat = glm::translate( glm::mat4( 1.0f ), m_translation );
        m_localMat *= glm::mat4_cast( m_rotation );
        m_localMat = glm::scale( m_localMat, m_scale );

        const glm::vec3 invScale( m_scale.x != 0.0f ? 1.0f / m_scale.x : 0.0f, m_scale.y != 0.0f ? 1.0f / m_scale.y : 0.0f,
                                  m_scale.z != 0.0f ? 1.0f / m_scale.z : 0.0f );
        const glm::mat4 invS = glm::scale( glm::mat4( 1.0f ), invScale );
        const glm::mat4 invR = glm::mat4_cast( glm::conjugate( m_rotation ) );
        const glm::mat4 invT = glm::translate( glm::mat4( 1.0f ), -m_translation );

        m_localInvMat = invS * invR * invT;
        m_localDirty  = false;
    }

    bool TTransform::updateGlobal( const TTransform& p_parent, bool p_parentDirty )
    {
        const bool needsUpdate = m_globalDirty || p_parentDirty;
        if ( !needsUpdate ) return false;

        if ( m_localDirty ) updateLocal();

        m_globMat     = p_parent.m_globMat * m_localMat;
        m_globInvMat  = m_localInvMat * p_parent.m_globInvMat;
        m_globalDirty = false;
        return true;
    }

    const glm::mat4& TTransform::getLocalMatrix() const
    {
        if ( m_localDirty ) updateLocal();
        return m_localMat;
    }

    const glm::mat4& TTransform::getLocalInvMatrix() const
    {
        if ( m_localDirty ) updateLocal();
        return m_localInvMat;
    }
}  // namespace Tomos
