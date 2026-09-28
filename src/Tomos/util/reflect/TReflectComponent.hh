#pragma once

#include <functional>
#include <memory>
#include <string>
#include <type_traits>

#include "Tomos/systems/TComponent.hh"
#include "Tomos/systems/TComponentRegistry.hh"
#include "Tomos/util/reflect/TReflectFields.hh"

namespace Tomos::Reflect
{
    // Exact dynamic_cast match; optionally exclude a derived type (mesh vs skinned).
    template<typename T, typename Exclude = void>
    std::function<bool( const TComponent& )> makeMatcher()
    {
        return []( const TComponent& p_c )
        {
            if ( dynamic_cast<const T*>( &p_c ) == nullptr ) return false;
            if constexpr ( !std::is_void_v<Exclude> )
            {
                if ( dynamic_cast<const Exclude*>( &p_c ) != nullptr ) return false;
            }
            return true;
        };
    }

    template<typename T, typename Exclude = void>
    TComponentTypeInfo makeComponentStub( const char*                                                                               p_type,
                                          const char*                                                                               p_label,
                                          std::function<nlohmann::json( const TComponent&, const TComponentResolveCtx& )>           p_save,
                                          std::function<std::unique_ptr<TComponent>( const nlohmann::json&, TComponentResolveCtx& )> p_load,
                                          std::function<std::unique_ptr<TComponent>()>                                              p_create = nullptr )
    {
        if ( !p_create )
        {
            if constexpr ( std::is_default_constructible_v<T> )
            {
                p_create = [] { return std::unique_ptr<TComponent>( std::make_unique<T>() ); };
            }
        }

        return TComponentTypeInfo{
                .m_type    = p_type,
                .m_label   = p_label,
                .m_create  = std::move( p_create ),
                .m_matches = makeMatcher<T, Exclude>(),
                .m_save    = std::move( p_save ),
                .m_load    = std::move( p_load ),
        };
    }

#if defined( __cpp_impl_reflection ) && __cpp_impl_reflection >= 202400L

    template<typename T, typename Exclude = void>
        requires( hasAnnotation<ComponentMeta>( ^^T ) )
    TComponentTypeInfo makeComponentStub( std::function<nlohmann::json( const TComponent&, const TComponentResolveCtx& )>           p_save,
                                          std::function<std::unique_ptr<TComponent>( const nlohmann::json&, TComponentResolveCtx& )> p_load,
                                          std::function<std::unique_ptr<TComponent>()>                                              p_create = nullptr )
    {
        constexpr auto meta = componentMeta<T>();
        return makeComponentStub<T, Exclude>( meta.type, meta.label, std::move( p_save ), std::move( p_load ), std::move( p_create ) );
    }

    // POD components: reflected field JSON + optional post-load hook (setDirty / setMass / …).
    template<typename T>
        requires( hasAnnotation<ComponentMeta>( ^^T ) )
    TComponentTypeInfo makePodComponent( void ( *p_postLoad )( T& ) = nullptr )
    {
        return makeComponentStub<T>(
                []( const TComponent& p_c, const TComponentResolveCtx& )
                { return saveFields( dynamic_cast<const T&>( p_c ) ); },
                [ p_postLoad ]( const nlohmann::json& p_j, TComponentResolveCtx& ) -> std::unique_ptr<TComponent>
                {
                    auto obj = std::make_unique<T>();
                    loadFields( *obj, p_j );
                    if ( p_postLoad != nullptr ) p_postLoad( *obj );
                    return obj;
                } );
    }

#else

    // clangd stubs — not used by real GCC builds.
    template<typename T, typename Exclude = void>
    TComponentTypeInfo makeComponentStub( std::function<nlohmann::json( const TComponent&, const TComponentResolveCtx& )>           p_save,
                                          std::function<std::unique_ptr<TComponent>( const nlohmann::json&, TComponentResolveCtx& )> p_load,
                                          std::function<std::unique_ptr<TComponent>()>                                              p_create = nullptr )
    {
        return makeComponentStub<T, Exclude>( "?", "?", std::move( p_save ), std::move( p_load ), std::move( p_create ) );
    }

    template<typename T>
    TComponentTypeInfo makePodComponent( void ( * )( T& ) = nullptr )
    {
        return makeComponentStub<T>( "?", "?", nullptr, nullptr, nullptr );
    }

#endif
}  // namespace Tomos::Reflect
