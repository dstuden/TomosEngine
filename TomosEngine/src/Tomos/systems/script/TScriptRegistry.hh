#pragma once

#include <functional>
#include <memory>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <vector>

#include "Tomos/systems/script/TScript.hh"

namespace Tomos
{
    // type name → default-constructible TScript (for TSceneSerializer).
    class TScriptRegistry
    {
    public:
        using Factory = std::function<std::unique_ptr<TScript>()>;

        static TScriptRegistry& get();

        void registerType( const std::string& p_name, Factory p_factory );

        template<typename T>
        void registerType( const std::string& p_name )
        {
            static_assert( std::is_base_of_v<TScript, T>, "T must derive from TScript" );
            registerType( p_name, [] { return std::make_unique<T>(); } );
        }

        [[nodiscard]] bool                        has( const std::string& p_name ) const;
        [[nodiscard]] std::unique_ptr<TScript>    create( const std::string& p_name ) const;
        [[nodiscard]] const std::vector<std::string>& names() const { return m_names; }

    private:
        TScriptRegistry() = default;

        std::unordered_map<std::string, Factory> m_factories;
        std::vector<std::string>                 m_names;
    };
}  // namespace Tomos
