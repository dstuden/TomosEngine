#pragma once

#include "Tomos/core/TLayer.hh"

namespace Tomos
{

    class ImGuiLayer : public TLayer
    {
    public:
        ImGuiLayer( const std::string& p_name = "ImGuiLayer" );
        ~ImGuiLayer() override;

        void onUpdate() override;
        void onEvent( TEvent& p_event ) override;

        void onAttach() override;
        void onDetach() override;

    protected:
        float m_time = 0.0f;
    };

}  // namespace Tomos
