#pragma once

// Avoids circular include between TScript and TScriptComponent.
#include <stdexcept>

#include "Tomos/core/scene/TSceneNode.hh"
#include "Tomos/systems/script/TScript.hh"
#include "Tomos/systems/script/TScriptComponent.hh"

namespace Tomos::TScriptUtils
{
    template<typename T>
    [[nodiscard]] T* findScript( TSceneNode& p_node )
    {
        static_assert( std::is_base_of_v<TScript, T>, "T must derive from TScript" );
        for ( auto& comp : p_node.getComponents() )
        {
            if ( auto* sc = dynamic_cast<TScriptComponent*>( comp.get() ) )
            {
                if ( auto* script = dynamic_cast<T*>( &sc->script() ) ) return script;
            }
        }
        return nullptr;
    }

    template<typename T>
    [[nodiscard]] T& requireScript( TSceneNode& p_node )
    {
        T* ptr = findScript<T>( p_node );
        if ( ptr == nullptr ) throw std::runtime_error( "[TScriptUtils] Required script not found on node: " + p_node.m_name );
        return *ptr;
    }
}  // namespace Tomos::TScriptUtils
