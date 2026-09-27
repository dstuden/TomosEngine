#pragma once

#include <optional>
#include <string_view>

#include "Tomos/util/reflect/TReflectMeta.hh"

#if !defined( __cpp_impl_reflection ) || __cpp_impl_reflection < 202400L

namespace Tomos::Reflect
{
    template<typename E>
    constexpr std::string_view enumName( E ) { return {}; }
    template<typename E>
    constexpr std::string_view enumNameLower( E ) { return {}; }
    template<typename E>
    constexpr std::optional<E> enumParseLower( std::string_view ) { return std::nullopt; }
    template<typename E>
    constexpr E enumParseLowerOr( std::string_view, E p_fallback ) { return p_fallback; }
    template<typename E>
    constexpr int enumIndex( E ) { return 0; }
    template<typename E>
    constexpr E enumFromIndex( int ) { return E{}; }
    template<typename E>
    consteval std::size_t enumCount() { return 0; }
    template<typename E>
    consteval const char* enumImGuiItems() { return ""; }
    template<typename E>
    constexpr const char* enumNameCStr( E ) { return "?"; }
}  // namespace Tomos::Reflect

#else

#include <meta>

namespace Tomos::Reflect
{
    template<typename E>
    consteval auto enumerators()
    {
        return std::define_static_array( std::meta::enumerators_of( ^^E ) );
    }

    consteval std::string_view toLowerStatic( std::string_view p_s )
    {
        char         buf[ 64 ]{};
        const size_t n = p_s.size() < 63 ? p_s.size() : 63;
        for ( size_t i = 0; i < n; ++i )
        {
            char c = p_s[ i ];
            if ( c >= 'A' && c <= 'Z' ) c = static_cast<char>( c - 'A' + 'a' );
            buf[ i ] = c;
        }
        return std::define_static_string( std::string_view( buf, n ) );
    }

    template<typename E>
    constexpr std::string_view enumName( E p_v )
    {
        template for ( constexpr auto e : enumerators<E>() )
        {
            if ( p_v == [:e:] ) return std::meta::identifier_of( e );
        }
        return {};
    }

    template<typename E>
    constexpr std::string_view enumNameLower( E p_v )
    {
        template for ( constexpr auto e : enumerators<E>() )
        {
            if ( p_v == [:e:] ) return toLowerStatic( std::meta::identifier_of( e ) );
        }
        return {};
    }

    template<typename E>
    constexpr std::optional<E> enumParseLower( std::string_view p_name )
    {
        template for ( constexpr auto e : enumerators<E>() )
        {
            if ( p_name == toLowerStatic( std::meta::identifier_of( e ) ) ) return [:e:];
        }
        return std::nullopt;
    }

    template<typename E>
    constexpr E enumParseLowerOr( std::string_view p_name, E p_fallback )
    {
        if ( auto v = enumParseLower<E>( p_name ) ) return *v;
        return p_fallback;
    }

    template<typename E>
    constexpr int enumIndex( E p_v )
    {
        int i = 0;
        template for ( constexpr auto e : enumerators<E>() )
        {
            if ( p_v == [:e:] ) return i;
            ++i;
        }
        return 0;
    }

    template<typename E>
    constexpr E enumFromIndex( int p_idx )
    {
        int i = 0;
        template for ( constexpr auto e : enumerators<E>() )
        {
            if ( i == p_idx ) return [:e:];
            ++i;
        }
        return E{};
    }

    template<typename E>
    consteval std::size_t enumCount()
    {
        return enumerators<E>().size();
    }

    template<typename E>
    consteval const char* enumImGuiItems()
    {
        char   buf[ 1024 ]{};
        size_t pos = 0;
        template for ( constexpr auto e : enumerators<E>() )
        {
            const auto id = std::meta::identifier_of( e );
            for ( char c : id )
            {
                if ( pos + 1 < sizeof( buf ) ) buf[ pos++ ] = c;
            }
            if ( pos + 1 < sizeof( buf ) ) buf[ pos++ ] = '\0';
        }
        return std::define_static_string( std::string_view( buf, pos ) );
    }

    template<typename E>
    constexpr const char* enumNameCStr( E p_v )
    {
        template for ( constexpr auto e : enumerators<E>() )
        {
            if ( p_v == [:e:] ) return std::define_static_string( std::meta::identifier_of( e ) );
        }
        return "?";
    }
}  // namespace Tomos::Reflect

#endif
