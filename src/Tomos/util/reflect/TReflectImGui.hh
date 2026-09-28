#pragma once

#include <algorithm>
#include <cmath>
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <imgui.h>
#include <string>
#include <type_traits>

#include "Tomos/util/reflect/TReflectEnum.hh"

#if !defined( __cpp_impl_reflection ) || __cpp_impl_reflection < 202400L

namespace Tomos::Reflect
{
    template<typename T>
    bool editFields( T& )
    {
        return false;
    }
}  // namespace Tomos::Reflect

#else

namespace Tomos::Reflect
{
    // Draw public scalar/enum/vec fields. Returns true if any widget changed the value.
    // Skips Reflect::Skip and Reflect::UiSkip. Callers handle asset pickers / side effects.
    template<typename T>
    bool editFields( T& p_obj )
    {
        bool changed = false;
        template for ( constexpr auto m : publicMembers<T>() )
        {
            if constexpr ( !hasAnnotation<Skip>( m ) && !hasAnnotation<UiSkip>( m ) )
            {
                using MT               = [:std::meta::type_of( m ):];
                constexpr auto labelSv = memberUiLabel<m>();
                const char*    label   = labelSv.data();

                if constexpr ( std::is_same_v<MT, bool> )
                {
                    changed = ImGui::Checkbox( label, &p_obj.[:m:] ) || changed;
                }
                else if constexpr ( std::is_enum_v<MT> )
                {
                    int idx = enumIndex( p_obj.[:m:] );
                    if ( ImGui::Combo( label, &idx, enumImGuiItems<MT>() ) )
                    {
                        p_obj.[:m:] = enumFromIndex<MT>( idx );
                        changed     = true;
                    }
                }
                else if constexpr ( std::is_same_v<MT, float> )
                {
                    if constexpr ( hasAnnotation<Degrees>( m ) )
                    {
                        float deg = p_obj.[:m:] * ( 180.0f / 3.14159265358979323846f );
                        float min = 0.0f, max = 180.0f;
                        if constexpr ( hasAnnotation<UiRange>( m ) )
                        {
                            constexpr auto r = getAnnotationOr<UiRange>( m, {} );
                            min              = r.min;
                            max              = r.max;
                        }
                        if ( ImGui::SliderFloat( label, &deg, min, max ) )
                        {
                            p_obj.[:m:] = deg * ( 3.14159265358979323846f / 180.0f );
                            changed     = true;
                        }
                    }
                    else if constexpr ( hasAnnotation<UiRange>( m ) )
                    {
                        constexpr auto r = getAnnotationOr<UiRange>( m, {} );
                        changed          = ImGui::SliderFloat( label, &p_obj.[:m:], r.min, r.max ) || changed;
                    }
                    else
                    {
                        changed = ImGui::DragFloat( label, &p_obj.[:m:], 0.05f ) || changed;
                    }
                }
                else if constexpr ( std::is_same_v<MT, int> )
                {
                    if constexpr ( hasAnnotation<UiRange>( m ) )
                    {
                        constexpr auto r = getAnnotationOr<UiRange>( m, {} );
                        changed          = ImGui::SliderInt( label, &p_obj.[:m:], static_cast<int>( r.min ), static_cast<int>( r.max ) ) || changed;
                    }
                    else
                    {
                        changed = ImGui::DragInt( label, &p_obj.[:m:] ) || changed;
                    }
                }
                else if constexpr ( std::is_same_v<MT, uint32_t> )
                {
                    int v = static_cast<int>( p_obj.[:m:] );
                    if constexpr ( hasAnnotation<UiRange>( m ) )
                    {
                        constexpr auto r = getAnnotationOr<UiRange>( m, {} );
                        if ( ImGui::SliderInt( label, &v, static_cast<int>( r.min ), static_cast<int>( r.max ) ) )
                        {
                            p_obj.[:m:] = static_cast<uint32_t>( std::max( 0, v ) );
                            changed     = true;
                        }
                    }
                    else if ( ImGui::DragInt( label, &v ) )
                    {
                        p_obj.[:m:] = static_cast<uint32_t>( std::max( 0, v ) );
                        changed     = true;
                    }
                }
                else if constexpr ( std::is_same_v<MT, glm::vec2> )
                {
                    changed = ImGui::DragFloat2( label, &p_obj.[:m:].x, 0.01f ) || changed;
                }
                else if constexpr ( std::is_same_v<MT, glm::vec3> )
                {
                    if constexpr ( hasAnnotation<UiColor>( m ) )
                        changed = ImGui::ColorEdit3( label, &p_obj.[:m:].x ) || changed;
                    else
                        changed = ImGui::DragFloat3( label, &p_obj.[:m:].x, 0.05f ) || changed;
                }
                else if constexpr ( std::is_same_v<MT, glm::vec4> )
                {
                    if constexpr ( hasAnnotation<UiColor>( m ) )
                        changed = ImGui::ColorEdit4( label, &p_obj.[:m:].x ) || changed;
                    else
                        changed = ImGui::DragFloat4( label, &p_obj.[:m:].x, 0.05f ) || changed;
                }
            }
        }
        return changed;
    }
}  // namespace Tomos::Reflect

#endif
