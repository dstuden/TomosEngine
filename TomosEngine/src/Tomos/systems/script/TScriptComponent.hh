#pragma once

#include <memory>
#include <string>
#include <utility>

#include "Tomos/systems/TComponent.hh"
#include "Tomos/systems/script/TScript.hh"
#include "Tomos/systems/script/TScriptRegistry.hh"

namespace Tomos
{
    class TScriptComponent : public TComponent
    {
    public:
        template<typename T, typename... Args>
        [[nodiscard]] static std::unique_ptr<TScriptComponent> make( Args&&... p_args )
        {
            static_assert( std::is_base_of_v<TScript, T>, "T must derive from TScript" );
            auto comp      = std::make_unique<TScriptComponent>();
            comp->m_script = std::make_unique<T>( std::forward<Args>( p_args )... );
            return comp;
        }

        [[nodiscard]] static std::unique_ptr<TScriptComponent> makeFromType( const std::string& p_typeName )
        {
            auto script = TScriptRegistry::get().create( p_typeName );
            if ( script == nullptr ) return nullptr;
            auto comp      = std::make_unique<TScriptComponent>();
            comp->m_script = std::move( script );
            return comp;
        }

        [[nodiscard]] TScript&       script() { return *m_script; }
        [[nodiscard]] const TScript& script() const { return *m_script; }

        [[nodiscard]] std::string typeName() const
        {
            if ( m_script == nullptr ) return {};
            return std::string( m_script->typeName() );
        }

    private:
        std::unique_ptr<TScript> m_script;
    };
}  // namespace Tomos
