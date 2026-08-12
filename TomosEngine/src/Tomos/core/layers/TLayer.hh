#pragma once

#include <string>

#include "Tomos/core/events/TEvent.hh"

namespace Tomos
{
    // Stack layer — updated bottom-to-top, events top-to-bottom.
    class TLayer
    {
    public:
        explicit TLayer( std::string p_name = "Layer" ) : m_name( std::move( p_name ) ) {}
        virtual ~TLayer() = default;

        TLayer( const TLayer& )            = delete;
        TLayer& operator=( const TLayer& ) = delete;

        virtual void onAttach() {}

        virtual void onDetach() {}

        virtual void onUpdate( float p_dt ) {}

        virtual void onRender() {}

        virtual void onEvent( TEvent& p_event ) {}

        [[nodiscard]] const std::string& name() const { return m_name; }

    protected:
        std::string m_name;
    };
}  // namespace Tomos
