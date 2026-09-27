#pragma once

#include <cstdint>
#include <glm/glm.hpp>
#include <nlohmann/json.hpp>
#include <string>
#include <string_view>
#include <type_traits>

#include "Tomos/util/reflect/TReflectEnum.hh"

#if !defined( __cpp_impl_reflection ) || __cpp_impl_reflection < 202400L

namespace Tomos::Reflect
{
    template<typename T>
    nlohmann::json saveFields( const T& ) { return nlohmann::json::object(); }

    template<typename T>
    void loadFields( T&, const nlohmann::json& ) {}
}  // namespace Tomos::Reflect

#else

namespace Tomos::Reflect
{
    namespace detail
    {
        inline nlohmann::json toJson( bool p_v ) { return p_v; }
        inline nlohmann::json toJson( int p_v ) { return p_v; }
        inline nlohmann::json toJson( std::int64_t p_v ) { return p_v; }
        inline nlohmann::json toJson( std::uint32_t p_v ) { return p_v; }
        inline nlohmann::json toJson( std::uint64_t p_v ) { return p_v; }
        inline nlohmann::json toJson( float p_v ) { return p_v; }
        inline nlohmann::json toJson( const glm::vec2& p_v ) { return nlohmann::json::array( { p_v.x, p_v.y } ); }
        inline nlohmann::json toJson( const glm::vec3& p_v ) { return nlohmann::json::array( { p_v.x, p_v.y, p_v.z } ); }
        inline nlohmann::json toJson( const glm::vec4& p_v ) { return nlohmann::json::array( { p_v.x, p_v.y, p_v.z, p_v.w } ); }

        template<typename E>
            requires std::is_enum_v<E>
        nlohmann::json toJson( E p_v )
        {
            return std::string( enumNameLower( p_v ) );
        }

        template<typename T>
        void fromJson( T& p_out, const nlohmann::json& p_j, const T& p_fallback )
        {
            if constexpr ( std::is_enum_v<T> )
            {
                if ( p_j.is_string() )
                    p_out = enumParseLowerOr<T>( p_j.get<std::string>(), p_fallback );
                else
                    p_out = p_fallback;
            }
            else if constexpr ( std::is_same_v<T, glm::vec2> )
            {
                if ( p_j.is_array() && p_j.size() >= 2 )
                    p_out = { p_j[ 0 ].get<float>(), p_j[ 1 ].get<float>() };
                else
                    p_out = p_fallback;
            }
            else if constexpr ( std::is_same_v<T, glm::vec3> )
            {
                if ( p_j.is_array() && p_j.size() >= 3 )
                    p_out = { p_j[ 0 ].get<float>(), p_j[ 1 ].get<float>(), p_j[ 2 ].get<float>() };
                else
                    p_out = p_fallback;
            }
            else if constexpr ( std::is_same_v<T, glm::vec4> )
            {
                if ( p_j.is_array() && p_j.size() >= 4 )
                    p_out = { p_j[ 0 ].get<float>(), p_j[ 1 ].get<float>(), p_j[ 2 ].get<float>(), p_j[ 3 ].get<float>() };
                else
                    p_out = p_fallback;
            }
            else if constexpr ( std::is_same_v<T, bool> )
            {
                p_out = p_j.is_boolean() ? p_j.get<bool>() : p_fallback;
            }
            else if constexpr ( std::is_integral_v<T> )
            {
                p_out = p_j.is_number_integer() || p_j.is_number_unsigned() ? p_j.get<T>() : p_fallback;
            }
            else if constexpr ( std::is_floating_point_v<T> )
            {
                p_out = p_j.is_number() ? p_j.get<T>() : p_fallback;
            }
            else
            {
                p_out = p_fallback;
            }
        }

        template<typename T>
        concept JsonField = std::is_enum_v<T> || std::is_same_v<T, bool> || std::is_integral_v<T> || std::is_floating_point_v<T>
                            || std::is_same_v<T, glm::vec2> || std::is_same_v<T, glm::vec3> || std::is_same_v<T, glm::vec4>;
    }  // namespace detail

    template<typename T>
    nlohmann::json saveFields( const T& p_obj )
    {
        nlohmann::json j = nlohmann::json::object();
        template for ( constexpr auto m : publicMembers<T>() )
        {
            if constexpr ( !hasAnnotation<Skip>( m ) )
            {
                using MT = [:std::meta::type_of( m ):];
                if constexpr ( detail::JsonField<MT> )
                {
                    constexpr auto key = memberJsonKey<m>();
                    j[ std::string( key ) ] = detail::toJson( p_obj.[:m:] );
                }
            }
        }
        return j;
    }

    template<typename T>
    void loadFields( T& p_obj, const nlohmann::json& p_j )
    {
        if ( !p_j.is_object() ) return;
        template for ( constexpr auto m : publicMembers<T>() )
        {
            if constexpr ( !hasAnnotation<Skip>( m ) )
            {
                using MT = [:std::meta::type_of( m ):];
                if constexpr ( detail::JsonField<MT> )
                {
                    constexpr auto key = memberJsonKey<m>();
                    const auto     it  = p_j.find( std::string( key ) );
                    if ( it != p_j.end() ) detail::fromJson( p_obj.[:m:], *it, p_obj.[:m:] );
                }
            }
        }
    }
}  // namespace Tomos::Reflect

#endif
