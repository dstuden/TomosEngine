#pragma once

#include <glm/glm.hpp>
#include <memory>
#include <stdexcept>

#include "../Component.hh"

namespace Tomos
{
    enum class LightType
    {
        Directional = 0,
        Point = 1,
        Spot = 2
    };

    struct LightInfo
    {
        glm::vec3 m_color{ 1.0f, 1.0f, 1.0f };
        float     m_intensity{ 1.0f };
        bool      m_castsShadows{ false };

        LightType m_type;
        virtual ~LightInfo() = default;
    };

    struct DirectionalLightInfo : public LightInfo
    {
        DirectionalLightInfo( const glm::vec3& p_color, float p_intensity = 1.0f, bool p_castsShadows = false)
        {
            m_color        = p_color;
            m_intensity    = p_intensity;
            m_castsShadows = p_castsShadows;
            m_type         = LightType::Directional;
        }
    };

    struct PointLightInfo : public LightInfo
    {
        float m_range{ 10.0f };
        PointLightInfo( const glm::vec3& p_color, float p_intensity = 1.0f, bool p_castsShadows = false, float p_range = 10.0f )
        {
            m_color        = p_color;
            m_intensity    = p_intensity;
            m_castsShadows = p_castsShadows;
            m_range        = p_range;
            m_type         = LightType::Point;
        }
    };

    struct SpotLightInfo : public LightInfo
    {
        float m_innerAngle{ 15.0f };
        float m_outerAngle{ 30.0f };
        float m_range{ 10.0f };
        SpotLightInfo( const glm::vec3& p_color, float p_intensity = 1.0f, bool p_castsShadows = false, float p_innerAngle = 15.0f, float p_outerAngle = 30.0f,
                       float p_range = 10.0f )
        {
            m_color        = p_color;
            m_intensity    = p_intensity;
            m_castsShadows = p_castsShadows;
            m_type         = LightType::Spot;
            m_innerAngle   = p_innerAngle;
            m_outerAngle   = p_outerAngle;
            m_range        = p_range;
        }
    };

    static std::string lightTypeToString( LightType p_type )
    {
        switch ( p_type )
        {
            case LightType::Directional:
                return "Directional";
            case LightType::Point:
                return "Point";
            case LightType::Spot:
                return "Spot";
            default:
                return "Directional";
        }
    }

    class LightComponent : public Component
    {
    public:
        LightComponent( std::shared_ptr<LightInfo> p_lightInfo, const std::string& p_name = "Light" ) : m_lightInfo( p_lightInfo )
        {
            m_name = lightTypeToString( p_lightInfo->m_type ) + "_" + p_name;
        }

        std::shared_ptr<LightInfo> getLightInfo() const { return m_lightInfo; }

    protected:
        std::shared_ptr<LightInfo> m_lightInfo;
    };

}  // namespace Tomos
