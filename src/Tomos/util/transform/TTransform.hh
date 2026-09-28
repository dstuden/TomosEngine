#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

namespace Tomos
{
    // getLocalMatrix() safe before computeTransforms; getGlobalMatrix() needs updateGlobal.
    class TTransform
    {
    public:
        explicit TTransform( const glm::vec3& p_translation = glm::vec3( 0.0f ), const glm::quat& p_rotation = glm::quat( 1.0f, 0.0f, 0.0f, 0.0f ),
                             const glm::vec3& p_scale = glm::vec3( 1.0f ) );

        void setDirty() const
        {
            m_localDirty  = true;
            m_globalDirty = true;
        }

        [[nodiscard]] const glm::vec3& translation() const { return m_translation; }
        [[nodiscard]] const glm::quat& rotation() const { return m_rotation; }
        [[nodiscard]] const glm::vec3& scale() const { return m_scale; }

        void setTranslation( const glm::vec3& p_translation )
        {
            m_translation = p_translation;
            setDirty();
        }

        void setRotation( const glm::quat& p_rotation )
        {
            m_rotation = p_rotation;
            setDirty();
        }

        void setScale( const glm::vec3& p_scale )
        {
            m_scale = p_scale;
            setDirty();
        }

        void setLocalTRS( const glm::vec3& p_translation, const glm::quat& p_rotation, const glm::vec3& p_scale )
        {
            m_translation = p_translation;
            m_rotation    = p_rotation;
            m_scale       = p_scale;
            setDirty();
        }

        void translate( const glm::vec3& p_delta )
        {
            m_translation += p_delta;
            setDirty();
        }

        bool updateGlobal( const TTransform& p_parent, bool p_parentDirty = false );

        const glm::mat4& getLocalMatrix() const;
        const glm::mat4& getLocalInvMatrix() const;
        const glm::mat4& getGlobalMatrix() const { return m_globMat; }
        const glm::mat4& getGlobalInvMatrix() const { return m_globInvMat; }

    private:
        void updateLocal() const;

        glm::vec3 m_translation;
        glm::quat m_rotation;
        glm::vec3 m_scale;

        mutable glm::mat4 m_localMat{ 1.0f };
        mutable glm::mat4 m_localInvMat{ 1.0f };
        mutable bool      m_localDirty{ true };
        mutable bool      m_globalDirty{ true };

        glm::mat4 m_globMat{ 1.0f };
        glm::mat4 m_globInvMat{ 1.0f };
    };
}  // namespace Tomos
