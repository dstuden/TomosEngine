#pragma once

#include <memory>

#include "TEcs.hh"
#include "TLayer.hh"
#include "TWindow.hh"
#include "Tomos/util/input/TInput.hh"
#include "Tomos/util/time/TTime.hh"

namespace Tomos
{
    class WindowCloseEvent;
    class TApplication;

    struct State
    {
    public:
        State() = default;

        ECS&        ecs() { return m_ecs; }
        LayerStack& layerStack() { return m_layerStack; }
        TInput&     input() { return m_input; }
        TTime&      time() { return m_time; }

    private:
        ECS m_ecs;

        LayerStack m_layerStack;

        TInput m_input;
        TTime  m_time;
    };

    class TApplication
    {
    public:
        static TApplication* get();
        static void          cleanup();
        static void          init( const WindowProps& p_props = WindowProps() );

        void run();

        void onEvent( TEvent& p_e );
        bool onWindowClose( WindowCloseEvent& p_e );

        TWindow& getWindow() const;

        static State& getState()
        {
            TLOG_ASSERT_MSG( g_instance, "Application is not initialized!" );

            return g_instance->m_state;
        }

    private:
        TApplication( const WindowProps& p_props = WindowProps() );

        TApplication( const TApplication& )            = delete;
        TApplication& operator=( const TApplication& ) = delete;

        std::unique_ptr<TWindow> m_window{};

        bool m_running = true;

        static TApplication*                   g_instance;
        State                                  m_state;
    };
}  // namespace Tomos
