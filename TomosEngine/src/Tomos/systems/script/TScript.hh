#pragma once

#include <stdexcept>
#include <string_view>

namespace Tomos
{
    class TSceneNode;
    class TScriptSystem;

    // Override typeName() for TScriptRegistry / scene JSON round-trip.
    class TScript
    {
    public:
        virtual ~TScript() = default;

        TScript( const TScript& )            = delete;
        TScript& operator=( const TScript& ) = delete;

        // Empty = not serialized.
        [[nodiscard]] virtual std::string_view typeName() const { return {}; }

        virtual void onAttach() {}

        virtual void onDetach() {}

        virtual void earlyUpdate( float /*p_dt*/ ) {}
        virtual void update( float /*p_dt*/ ) {}
        virtual void lateUpdate( float /*p_dt*/ ) {}

        // Throws if called before onAttach.
        [[nodiscard]] TSceneNode& node() const
        {
            if ( m_node == nullptr ) throw std::runtime_error( "[TScript] node() called while script is not attached" );
            return *m_node;
        }

    protected:
        TScript() = default;

    private:
        friend class TScriptSystem;
        TSceneNode* m_node = nullptr;
    };
}  // namespace Tomos
