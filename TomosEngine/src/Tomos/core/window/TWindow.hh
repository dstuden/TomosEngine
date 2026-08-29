#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <functional>
#include <string>
#include <utility>

#include "Tomos/core/events/TEvent.hh"

namespace Tomos
{
    enum class TWindowMode
    {
        Windowed,
        Borderless,
        Exclusive,
    };

    struct TWindowProps
    {
        std::string  m_title{};
        unsigned int m_width;
        unsigned int m_height;
        float        m_aspectRatio{ 16.0f / 9.0f };

        explicit TWindowProps( std::string p_title = "TomosEngine", unsigned int p_width = 1280, unsigned int p_height = 720,
                               float p_aspectRatio = 16.0f / 9.0f ) :
            m_title( std::move( p_title ) ), m_width( p_width ), m_height( p_height ), m_aspectRatio( p_aspectRatio )
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

        static void onUpdate();

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

        // Deferred to flushPendingWindowMode() at frame start.
        void                   setWindowMode( TWindowMode p_mode );
        bool                   flushPendingWindowMode();
        [[nodiscard]] TWindowMode windowMode() const;

        [[nodiscard]] int windowedWidth() const { return m_windowedW; }
        [[nodiscard]] int windowedHeight() const { return m_windowedH; }

        static TWindowMode        windowModeFromString( const std::string& p_s );
        static const char*        windowModeToString( TWindowMode p_mode );

    private:
        void setDefaultWindowIcon();
        void applyWindowMode( TWindowMode p_mode );
        void syncCachedSizes();
        void storeWindowedGeom();

        GLFWwindow* m_window{};

        struct TWindowData
        {
            std::string m_title{};
            int         m_width{}, m_height{};
            int         m_fbWidth{}, m_fbHeight{};  // swapchain authority
            TWindowMode m_windowMode{ TWindowMode::Windowed };
            float       m_aspectRatio{ 16.0f / 9.0f };

            TEventCallback m_eventCallback{};
        } m_data;

        // Position unused on Wayland.
        int m_windowedW = 1280;
        int m_windowedH = 720;
        int m_windowedX = 0;
        int m_windowedY = 0;

        TWindowMode m_pendingWindowMode    = TWindowMode::Windowed;
        bool        m_hasPendingWindowMode = false;

        float m_perfAccumDt   = 0.0f;
        float m_lastDt        = 0.0f;
        int   m_perfFrames    = 0;
        float m_titleRefreshS = 0.25f;
    };
}  // namespace Tomos
