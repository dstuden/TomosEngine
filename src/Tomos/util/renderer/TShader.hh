#pragma once

#include <glm/glm.hpp>
#include <string>

#include "Tomos/util/resourceManager/TResourceManager.hh"


namespace Tomos
{
    class TShader
    {
    public:
        unsigned int m_rendererId;
        unsigned int m_resourceId = TResourceManager::getNewResourceId();

        TShader( const std::string& p_vertexPath, const std::string& p_fragmentPath );
        ~TShader();

        void bind() const;

        void setBool( const std::string& p_name, bool p_value ) const;

        void setInt( const std::string& p_name, int p_value ) const;

        void setFloat( const std::string& p_name, float p_value ) const;

        void setVec2( const std::string& p_name, const glm::vec2& p_value ) const;

        void setVec2( const std::string& p_name, float p_x, float p_y ) const;

        void setVec3( const std::string& p_name, const glm::vec3& p_value ) const;

        void setVec3( const std::string& p_name, float p_x, float p_y, float p_z ) const;

        void setVec4( const std::string& p_name, const glm::vec4& p_value ) const;

        void setVec4( const std::string& p_name, float p_x, float p_y, float p_z, float p_w ) const;

        void setMat2( const std::string& p_name, const glm::mat2& p_mat ) const;

        void setMat3( const std::string& p_name, const glm::mat3& p_mat ) const;

        void setMat4( const std::string& p_name, const glm::mat4& p_mat ) const;
    };
} // namespace Tomos
