#pragma once

#include <memory>

#include "Tomos/core/layers/TLayer.hh"
#include "Tomos/ui/TUiBackend.hh"

namespace Tomos
{
    // UI overlay — build widgets in onUi() only (after scene update).
    class TUiLayer : public TLayer
    {
    public:
        explicit TUiLayer( std::unique_ptr<TUiBackend> p_backend, std::string p_name = "UI" );

        void onAttach() override;
        void onDetach() override;
        void onUpdate( float p_dt ) override;
        void onRender() override;
        void onEvent( TEvent& p_event ) override;

    protected:
        virtual void onUi( float p_dt ) {}

        [[nodiscard]] TUiBackend& backend() { return *m_backend; }

    private:
        std::unique_ptr<TUiBackend> m_backend;
    };
}  // namespace Tomos
