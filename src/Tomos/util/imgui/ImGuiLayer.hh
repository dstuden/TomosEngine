#pragma once

#include "Tomos/core/TLayer.hh"

namespace Tomos
{

    class ImGuiLayer : public TLayer
    {
    public:
        ImGuiLayer( const std::string& p_id = "ImGuiLayer" );
        ~ImGuiLayer() override;

        void onUpdate() override;
        void onEvent( TEvent& p_event ) override;

        void onAttach() override;
        void onDetach() override;

        void blockEvents( bool p_block = true );

    protected:
        float m_time        = 0.0f;
        bool  m_blockEvents = false;
    };

}  // namespace Tomos
