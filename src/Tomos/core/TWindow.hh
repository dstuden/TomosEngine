#pragma once

#include <GL/glew.h>
// make sure to include glew before glfw
#include <GLFW/glfw3.h>
#include <functional>
#include <string>

#include "Tomos/events/TEvent.hh"

namespace Tomos
{
    struct WindowProps
    {
        std::string  m_title{};
        unsigned int m_width;
        unsigned int m_height;
        bool         m_vsync{ false };
        float        m_aspectRatio{ 16.0f / 9.0f };

        explicit WindowProps( const std::string& p_title = "Tomos Engine", unsigned int p_width = 1280, unsigned int p_height = 720, bool p_vsync = false,
                              float p_aspectRatio = 16.0f / 9.0f ) :
            m_title( p_title ), m_width( p_width ), m_height( p_height ), m_vsync( p_vsync ), m_aspectRatio( p_aspectRatio )
        {
        }
    };

    class TWindow
    {
    private:
        struct WindowData;

    public:
        using EventCallback = std::function<void( TEvent& )>;

        explicit TWindow( const WindowProps& p_props );
        ~TWindow();

        void shutdown();

        void onUpdate();

        WindowData& getData() { return m_data; }

        void setEventCallback( const EventCallback& p_callback );

        GLFWwindow* getNativeWindow() const;

        enum class CursorMode
        {
            Normal   = GLFW_CURSOR_NORMAL,
            Hidden   = GLFW_CURSOR_HIDDEN,
            Disabled = GLFW_CURSOR_DISABLED,
            Captured = GLFW_CURSOR_CAPTURED
        };

        void setCursorMode( CursorMode mode ) { glfwSetInputMode( m_window, GLFW_CURSOR, static_cast<int>( mode ) ); }

        void setRawInput( bool p_yes ) { glfwSetInputMode( m_window, GLFW_RAW_MOUSE_MOTION, p_yes ); }

    private:
        GLFWwindow* m_window{};

        struct WindowData
        {
            std::string  m_title{};
            unsigned int m_width{}, m_height{};
            bool         m_vsync{ false };
            float        m_aspectRatio{ 16.0f / 9.0f };

            EventCallback m_eventCallback{};
        } m_data;
    };
}  // namespace Tomos
