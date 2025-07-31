#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

namespace Tomos
{
    class TTransform
    {
    public:
        explicit TTransform( const glm::vec3& p_translation = glm::vec3( 0.0f, 0.0f, 0.0f ),
                   const glm::quat& p_rotation    = glm::quat( 1.0f, 0.0f, 0.0f, 0.0f ),
                   const glm::vec3& p_scale       = glm::vec3( 1.0f ) );

        void update();
        void updateGlobal( const TTransform& p_parent );

        const glm::mat4& getLocalMatrix();
        const glm::mat4& getLocalInvMatrix();

        glm::mat4 m_globMat;
        glm::mat4 m_globInvMat;

        glm::vec3 m_translation;
        glm::quat m_rotation;
        glm::vec3 m_scale;

        const glm::mat4& getLocalMatrix() const { return m_localMat; }
        const glm::mat4& getLocalInvMatrix() const { return m_localInvMat; }

    private:
        void updateLocal();

        glm::mat4 m_localMat;
        glm::mat4 m_localInvMat;

        bool m_dirty;
    };
} // namespace Tomos
