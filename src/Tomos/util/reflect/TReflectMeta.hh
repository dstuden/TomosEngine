#pragma once

#include "Tomos/util/reflect/TReflectAttr.hh"

#if !defined( __cpp_impl_reflection ) || __cpp_impl_reflection < 202400L
// IDE / non-reflection compilers (clangd): keep includes parseable. Real builds
// use GCC 16+ -freflection and the implementation below.
namespace Tomos::Reflect
{
    template<typename>
    consteval bool hasAnnotation( auto ) { return false; }

    template<typename Ann>
    consteval Ann getAnnotationOr( auto, Ann p_fallback ) { return p_fallback; }

    template<typename>
    consteval ComponentMeta componentMeta() { return {}; }
}  // namespace Tomos::Reflect
#else

#include <meta>
#include <optional>
#include <string_view>
#include <vector>

namespace Tomos::Reflect
{
    template<typename Ann>
    consteval bool hasAnnotation( std::meta::info p_m )
    {
        // Promote to a static array so we never destroy a temporary
        // std::meta::annotations_of vector during consteval (buggy on early GCC 16).
        auto anns = std::define_static_array( std::meta::annotations_of( p_m ) );
        for ( auto a : anns )
        {
            if ( std::meta::remove_const( std::meta::dealias( std::meta::type_of( a ) ) ) == ^^Ann ) return true;
        }
        return false;
    }

    template<typename Ann>
    consteval Ann getAnnotationOr( std::meta::info p_m, Ann p_fallback )
    {
        auto anns = std::define_static_array( std::meta::annotations_of( p_m ) );
        for ( auto a : anns )
        {
            if ( std::meta::remove_const( std::meta::dealias( std::meta::type_of( a ) ) ) == ^^Ann )
                return std::meta::extract<Ann>( a );
        }
        return p_fallback;
    }

    template<typename T>
    consteval auto publicMembers()
    {
        std::vector<std::meta::info> out;
        for ( auto m : std::meta::nonstatic_data_members_of( ^^T, std::meta::access_context::unprivileged() ) )
        {
            if ( std::meta::is_public( m ) ) out.push_back( m );
        }
        return std::define_static_array( out );
    }

    consteval std::string_view stripMPrefix( std::string_view p_id )
    {
        if ( p_id.size() >= 2 && p_id[ 0 ] == 'm' && p_id[ 1 ] == '_' ) return std::define_static_string( p_id.substr( 2 ) );
        return p_id;
    }

    template<std::meta::info M>
    consteval std::string_view memberJsonKey()
    {
        if constexpr ( hasAnnotation<JsonKey>( M ) )
        {
            constexpr auto k = getAnnotationOr<JsonKey>( M, {} );
            return std::define_static_string( std::string_view( k.name ) );
        }
        else
        {
            return stripMPrefix( std::meta::identifier_of( M ) );
        }
    }

    template<std::meta::info M>
    consteval std::string_view memberUiLabel()
    {
        if constexpr ( hasAnnotation<UiLabel>( M ) )
        {
            constexpr auto k = getAnnotationOr<UiLabel>( M, {} );
            return std::define_static_string( std::string_view( k.name ) );
        }
        else
        {
            return memberJsonKey<M>();
        }
    }

    template<typename T>
    consteval ComponentMeta componentMeta()
    {
        if constexpr ( hasAnnotation<ComponentMeta>( ^^T ) ) return getAnnotationOr<ComponentMeta>( ^^T, {} );
        return {};
    }
}  // namespace Tomos::Reflect

#endif
