#pragma once

#include <glm/glm.hpp>

#include "Tomos/systems/TComponent.hh"

namespace Tomos
{
    enum class TLightType
    {
        Point,
        Directional,
        Spot,
    };

    class TLightComponent : public TComponent
    {
    public:
        TLightType m_type      = TLightType::Point;
        glm::vec3  m_color     = glm::vec3( 1.0f );
        float      m_intensity = 1.0f;

        // Also shadow cubemap far plane for point lights.
        float m_maxRange = 10.0f;

        float m_innerCone = glm::radians( 30.0f );
        float m_outerCone = glm::radians( 45.0f );

        bool m_castShadow = false;
    };
}  // namespace Tomos
