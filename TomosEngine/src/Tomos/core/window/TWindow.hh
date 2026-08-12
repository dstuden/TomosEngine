#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <functional>
#include <iostream>
#include <string>

#include "Tomos/core/events/TEvent.hh"

namespace Tomos
{
    struct TWindowProps
    {
        std::string  m_title{};
        unsigned int m_width;
        unsigned int m_height;
        bool         m_vsync{ false };
        float        m_aspectRatio{ 16.0f / 9.0f };

        explicit TWindowProps( const std::string& p_title = "TomosEngine", unsigned int p_width = 1280, unsigned int p_height = 720, bool p_vsync = false,
                               float p_aspectRatio = 16.0f / 9.0f ) :
            m_title( p_title ), m_width( p_width ), m_height( p_height ), m_vsync( p_vsync ), m_aspectRatio( p_aspectRatio )
        {
        }
    };

    class TWindow
    {
    private:
        struct TWindowData;

    public:
        using TEventCallback = std::function<void( TEvent& )>;

        explicit TWindow( const TWindowProps& p_props );
        ~TWindow();

        void shutdown();

        void onUpdate();

        // FPS/frame time → window title.
        void updatePerfStats( float p_dt );

        TWindowData& getData() { return m_data; }

        void setEventCallback( const TEventCallback& p_callback );

        [[nodiscard]] GLFWwindow* getNativeWindow() const;

        enum class TCursorMode
        {
            Normal   = GLFW_CURSOR_NORMAL,
            Hidden   = GLFW_CURSOR_HIDDEN,
            Disabled = GLFW_CURSOR_DISABLED,
            Captured = GLFW_CURSOR_CAPTURED
        };

        void setCursorMode( TCursorMode p_mode ) { glfwSetInputMode( m_window, GLFW_CURSOR, static_cast<int>( p_mode ) ); }

        // Applied next frame (keeps ImGui DisplaySize and swapchain in sync).
        void setFullscreen( bool p_fullscreen );
        bool               flushPendingFullscreen();
        [[nodiscard]] bool isFullscreen() const;

    private:
        void setDefaultWindowIcon();
        void applyFullscreen( bool p_fullscreen );

        GLFWwindow* m_window{};

        struct TWindowData
        {
            std::string m_title{};
            int         m_width{}, m_height{};
            int         m_fbWidth{}, m_fbHeight{};  // swapchain authority
            bool        m_vsync{ false };
            bool        m_fullscreen{ false };
            float       m_aspectRatio{ 16.0f / 9.0f };

            TEventCallback m_eventCallback{};
        } m_data;

        // Position unused on Wayland.
        int m_windowedW = 1280;
        int m_windowedH = 720;

        bool m_pendingFullscreen    = false;
        bool m_hasPendingFullscreen = false;

        float m_perfAccumDt   = 0.0f;
        float m_lastDt        = 0.0f;
        int   m_perfFrames    = 0;
        float m_titleRefreshS = 0.25f;
    };
}  // namespace Tomos
