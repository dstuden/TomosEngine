#pragma once

#include <glm/glm.hpp>

#include "Tomos/systems/TComponent.hh"
#include "Tomos/util/reflect/TReflectAttr.hh"

namespace Tomos
{
    enum class TLightType
    {
        Point,
        Directional,
        Spot,
    };

    class TOMOS_ANN( Reflect::ComponentMeta{ "light", "Light" } ) TLightComponent : public TComponent
    {
    public:
        TOMOS_ANN( Reflect::JsonKey{ "lightType" } ) TOMOS_ANN( Reflect::UiLabel{ "Type" } ) TLightType m_type = TLightType::Point;
        TOMOS_ANN( Reflect::UiColor{} ) glm::vec3                                              m_color = glm::vec3( 1.0f );
        TOMOS_ANN( Reflect::UiRange{ 0.0f, 1000.0f } ) float                                   m_intensity = 1.0f;

        // Also shadow cubemap far plane for point lights.
        TOMOS_ANN( Reflect::UiLabel{ "Max range" } ) TOMOS_ANN( Reflect::UiRange{ 0.1f, 1000.0f } ) float m_maxRange = 10.0f;

        TOMOS_ANN( Reflect::Degrees{} ) TOMOS_ANN( Reflect::UiRange{ 0.0f, 89.0f } ) TOMOS_ANN( Reflect::UiLabel{ "Inner cone (deg)" } ) float m_innerCone =
                glm::radians( 30.0f );
        TOMOS_ANN( Reflect::Degrees{} ) TOMOS_ANN( Reflect::UiRange{ 1.0f, 90.0f } ) TOMOS_ANN( Reflect::UiLabel{ "Outer cone (deg)" } ) float m_outerCone =
                glm::radians( 45.0f );

        TOMOS_ANN( Reflect::UiLabel{ "Cast shadow" } ) bool m_castShadow = false;
    };
}  // namespace Tomos
