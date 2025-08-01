//
// Created by dstuden on 1/19/25.
//
#include "TWindow.hh"

#include "Tomos/events/application/TApplicationEvent.hh"
#include "Tomos/events/key/TKeyEvent.hh"
#include "Tomos/events/mouse/TMouseEvent.hh"
#include "Tomos/util/logger/TLogger.hh"


namespace Tomos
{
    TWindow::TWindow( const WindowProps& p_props )
    {
        m_data.m_title       = p_props.m_title;
        m_data.m_width       = p_props.m_width;
        m_data.m_height      = p_props.m_height;
        m_data.m_vsync       = p_props.m_vsync;
        m_data.m_aspectRatio = p_props.m_aspectRatio;

        TLOG_INFO() << "Creating m_window " << m_data.m_title << " (" << m_data.m_width << ", " << m_data.m_height << ")";

        if ( !glfwInit() )
        {
            TLOG_ERROR() << "Failed to initialize GLFW!";
            exit( EXIT_FAILURE );
        }

        glfwWindowHint( GLFW_CONTEXT_VERSION_MAJOR, 4 );
        glfwWindowHint( GLFW_CONTEXT_VERSION_MINOR, 3 );
        glfwWindowHint( GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE );
        glfwWindowHint( GLFW_SCALE_FRAMEBUFFER, false );

        m_window = glfwCreateWindow( m_data.m_width, m_data.m_height, m_data.m_title.c_str(), nullptr, nullptr );
        glfwMakeContextCurrent( m_window );
        glfwSetWindowUserPointer( m_window, &m_data );

        if ( glfwRawMouseMotionSupported() ) glfwSetInputMode( m_window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE );

        glewExperimental = GL_TRUE;
        auto res         = glewInit();
        if ( res != GLEW_OK && res != GLEW_ERROR_NO_GLX_DISPLAY )
        {
            TLOG_ERROR() << "Failed to initialize GLEW: " << glewGetErrorString( res );
            exit( EXIT_FAILURE );
        }

        TLOG_INFO() << "OpenGL Version: " << glGetString( GL_VERSION );

        glfwSetErrorCallback( []( int p_error, const char* p_description ) { TLOG_ERROR() << "GLFW Error (" << p_error << "): " << p_description; } );

        glfwSetWindowSizeCallback( m_window,
                                   []( GLFWwindow* p_window, int p_width, int p_height )
                                   {
                                       WindowData& data = *( WindowData* ) glfwGetWindowUserPointer( p_window );
                                       data.m_width     = p_width;
                                       data.m_height    = p_height;

                                       WindowResizeEvent event( p_width, p_height );
                                       data.m_eventCallback( event );
                                   } );

        glfwSetWindowCloseCallback( m_window,
                                    []( GLFWwindow* p_window )
                                    {
                                        WindowData&      data = *( WindowData* ) glfwGetWindowUserPointer( p_window );
                                        WindowCloseEvent event;
                                        data.m_eventCallback( event );
                                    } );

        glfwSetKeyCallback( m_window,
                            []( GLFWwindow* p_window, int p_key, int p_scancode, int p_action, int p_mods )
                            {
                                WindowData& data = *( WindowData* ) glfwGetWindowUserPointer( p_window );

                                switch ( p_action )
                                {
                                    case GLFW_PRESS:
                                    {
                                        KeyPressedEvent event( p_key, 0 );
                                        data.m_eventCallback( event );
                                        break;
                                    }
                                    case GLFW_RELEASE:
                                    {
                                        KeyReleasedEvent event( p_key );
                                        data.m_eventCallback( event );
                                        break;
                                    }
                                    case GLFW_REPEAT:
                                    {
                                        KeyPressedEvent event( p_key, 1 );
                                        data.m_eventCallback( event );
                                        break;
                                    }
                                }
                            } );

        glfwSetMouseButtonCallback( m_window,
                                    []( GLFWwindow* p_window, int p_button, int p_action, int p_mods )
                                    {
                                        WindowData& data = *( WindowData* ) glfwGetWindowUserPointer( p_window );

                                        switch ( p_action )
                                        {
                                            case GLFW_PRESS:
                                            {
                                                MouseButtonPressedEvent event( p_button );
                                                data.m_eventCallback( event );
                                                break;
                                            }
                                            case GLFW_RELEASE:
                                            {
                                                MouseButtonReleasedEvent event( p_button );
                                                data.m_eventCallback( event );
                                                break;
                                            }
                                        }
                                    } );

        glfwSetScrollCallback( m_window,
                               []( GLFWwindow* p_window, double p_xOffset, double p_yOffset )
                               {
                                   WindowData& data = *( WindowData* ) glfwGetWindowUserPointer( p_window );

                                   MouseScrolledEvent event( p_xOffset, p_yOffset );
                                   data.m_eventCallback( event );
                               } );

        glfwSetCursorPosCallback( m_window,
                                  []( GLFWwindow* p_window, double p_xPos, double p_yPos )
                                  {
                                      WindowData& data = *( WindowData* ) glfwGetWindowUserPointer( p_window );

                                      MouseMovedEvent event( p_xPos, p_yPos );
                                      data.m_eventCallback( event );
                                  } );
    }

    TWindow::~TWindow() { shutdown(); }

    void TWindow::shutdown()
    {
        // We only support one m_window for now
        glfwDestroyWindow( m_window );
        glfwTerminate();
    }

    void TWindow::onUpdate()
    {
        glfwPollEvents();
        glfwSwapBuffers( m_window );
    }

    void TWindow::setEventCallback( const EventCallback& p_callback ) { m_data.m_eventCallback = p_callback; }

    GLFWwindow* TWindow::getNativeWindow() const { return m_window; }
}  // namespace Tomos
