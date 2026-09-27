#include "TWindow.hh"

#include <filesystem>
#include <format>
#include <stb/stb_image.h>
#include <stdexcept>

#include "Tomos/core/events/application/TApplicationEvent.hh"
#include "Tomos/core/events/key/TKeyEvent.hh"
#include "Tomos/core/events/mouse/TMouseEvent.hh"
#include "Tomos/util/logger/TLogger.hh"
#include "Tomos/util/reflect/TReflectEnum.hh"

namespace Tomos
{
    namespace
    {
        std::string findLogoPath()
        {
            static const char* kCandidates[] = {
#ifdef TOMOS_RESOURCES_DIR
                    TOMOS_RESOURCES_DIR "/Logo.png",
#endif
            };
            for ( const char* path : kCandidates )
            {
                if ( path != nullptr && std::filesystem::exists( path ) ) return path;
            }
            return {};
        }
    }  // namespace

    TWindowMode TWindow::windowModeFromString( const std::string& p_s )
    {
        return Reflect::enumParseLowerOr<TWindowMode>( p_s, TWindowMode::Windowed );
    }

    const char* TWindow::windowModeToString( TWindowMode p_mode )
    {
        const auto name = Reflect::enumNameLower( p_mode );
        return name.empty() ? "windowed" : name.data();
    }

    void TWindow::setDefaultWindowIcon()
    {
        if ( m_window == nullptr ) return;

        const std::string path = findLogoPath();
        if ( path.empty() )
        {
            TLOG_WARN() << "Window icon not found (tried resources/Logo.png)";
            return;
        }

        int      w = 0, h = 0, ch = 0;
        stbi_uc* pixels = stbi_load( path.c_str(), &w, &h, &ch, 4 );
        if ( pixels == nullptr || w <= 0 || h <= 0 )
        {
            TLOG_WARN() << "Failed to load window icon: " << path;
            return;
        }

        GLFWimage image{};
        image.width  = w;
        image.height = h;
        image.pixels = pixels;
        glfwSetWindowIcon( m_window, 1, &image );
        stbi_image_free( pixels );
    }

    TWindow::TWindow( const TWindowProps& p_props )
    {
        m_data.m_title       = p_props.m_title;
        m_data.m_width       = p_props.m_width;
        m_data.m_height      = p_props.m_height;
        m_data.m_aspectRatio = p_props.m_aspectRatio;
        m_windowedW          = static_cast<int>( p_props.m_width );
        m_windowedH          = static_cast<int>( p_props.m_height );

        TLOG_INFO() << format( "Creating window: '{}' ({}x{})", m_data.m_title, m_data.m_width, m_data.m_height );

        if ( !glfwInit() )
        {
            TLOG_ERROR() << "Failed to initialize GLFW!";
            throw std::runtime_error( "[TWindow] Failed to initialize GLFW" );
        }

        glfwWindowHint( GLFW_CLIENT_API, GLFW_NO_API );
        glfwWindowHint( GLFW_RESIZABLE, GLFW_TRUE );

        m_window = glfwCreateWindow( m_data.m_width, m_data.m_height, m_data.m_title.c_str(), nullptr, nullptr );
        if ( m_window == nullptr )
        {
            TLOG_ERROR() << "Failed to create GLFW window!";
            glfwTerminate();
            throw std::runtime_error( "[TWindow] Failed to create GLFW window" );
        }
        glfwSetWindowUserPointer( m_window, &m_data );
        glfwGetFramebufferSize( m_window, &m_data.m_fbWidth, &m_data.m_fbHeight );
        glfwGetWindowPos( m_window, &m_windowedX, &m_windowedY );
        setDefaultWindowIcon();

        glfwSetErrorCallback( []( int p_error, const char* p_description ) { TLOG_ERROR() << "GLFW Error (" << p_error << "): " << p_description; } );

        // Logical size for UI/config — does not drive the swapchain.
        glfwSetWindowSizeCallback( m_window,
                                   []( GLFWwindow* p_window, int p_width, int p_height )
                                   {
                                       TWindowData& data = *static_cast<TWindowData*>( glfwGetWindowUserPointer( p_window ) );
                                       data.m_width      = p_width;
                                       data.m_height     = p_height;
                                   } );

        // Framebuffer size is the sole swapchain / surface authority (HiDPI-safe).
        glfwSetFramebufferSizeCallback( m_window,
                                        []( GLFWwindow* p_window, int p_width, int p_height )
                                        {
                                            TWindowData& data = *static_cast<TWindowData*>( glfwGetWindowUserPointer( p_window ) );
                                            data.m_fbWidth    = p_width;
                                            data.m_fbHeight   = p_height;

                                            TWindowResizeEvent event( p_width, p_height );
                                            if ( data.m_eventCallback ) data.m_eventCallback( event );
                                        } );

        glfwSetWindowCloseCallback( m_window,
                                    []( GLFWwindow* p_window )
                                    {
                                        TWindowData&      data = *static_cast<TWindowData*>( glfwGetWindowUserPointer( p_window ) );
                                        TWindowCloseEvent event;
                                        data.m_eventCallback( event );
                                    } );

        glfwSetKeyCallback( m_window,
                            []( GLFWwindow* p_window, int p_key, int p_scancode, int p_action, int p_mods )
                            {
                                TWindowData& data = *static_cast<TWindowData*>( glfwGetWindowUserPointer( p_window ) );

                                switch ( p_action )
                                {
                                    case GLFW_PRESS:
                                    {
                                        TKeyPressedEvent event( p_key, 0 );
                                        data.m_eventCallback( event );
                                        break;
                                    }
                                    case GLFW_RELEASE:
                                    {
                                        TKeyReleasedEvent event( p_key );
                                        data.m_eventCallback( event );
                                        break;
                                    }
                                    case GLFW_REPEAT:
                                    {
                                        TKeyPressedEvent event( p_key, 1 );
                                        data.m_eventCallback( event );
                                        break;
                                    }
                                }
                            } );

        glfwSetMouseButtonCallback( m_window,
                                    []( GLFWwindow* p_window, int p_button, int p_action, int p_mods )
                                    {
                                        TWindowData& data = *static_cast<TWindowData*>( glfwGetWindowUserPointer( p_window ) );

                                        switch ( p_action )
                                        {
                                            case GLFW_PRESS:
                                            {
                                                TMouseButtonPressedEvent event( p_button );
                                                data.m_eventCallback( event );
                                                break;
                                            }
                                            case GLFW_RELEASE:
                                            {
                                                TMouseButtonReleasedEvent event( p_button );
                                                data.m_eventCallback( event );
                                                break;
                                            }
                                        }
                                    } );

        glfwSetScrollCallback( m_window,
                               []( GLFWwindow* p_window, double p_xOffset, double p_yOffset )
                               {
                                   TWindowData& data = *static_cast<TWindowData*>( glfwGetWindowUserPointer( p_window ) );

                                   TMouseScrolledEvent event( p_xOffset, p_yOffset );
                                   data.m_eventCallback( event );
                               } );

        glfwSetCursorPosCallback( m_window,
                                  []( GLFWwindow* p_window, double p_xPos, double p_yPos )
                                  {
                                      TWindowData& data = *static_cast<TWindowData*>( glfwGetWindowUserPointer( p_window ) );

                                      TMouseMovedEvent event( p_xPos, p_yPos );
                                      data.m_eventCallback( event );
                                  } );
    }

    TWindow::~TWindow() { shutdown(); }

    void TWindow::shutdown()
    {
        glfwDestroyWindow( m_window );
        glfwTerminate();
    }

    void TWindow::onUpdate() { glfwPollEvents(); }

    void TWindow::updatePerfStats( float p_dt )
    {
        if ( m_window == nullptr || p_dt <= 0.0f ) return;

        m_lastDt = p_dt;
        m_perfAccumDt += p_dt;
        ++m_perfFrames;

        if ( m_perfAccumDt < m_titleRefreshS ) return;

        const float fps     = 1.0f / m_lastDt;
        const float avgFps  = static_cast<float>( m_perfFrames ) / m_perfAccumDt;
        const float frameMs = ( m_perfAccumDt / static_cast<float>( m_perfFrames ) ) * 1000.0f;

        auto title = std::format( "{}  |  {:.0f} fps  |  avg {:.0f} fps  |  {:.2f} ms", m_data.m_title, fps, avgFps, frameMs );
        glfwSetWindowTitle( m_window, title.c_str() );

        m_perfAccumDt = 0.0f;
        m_perfFrames  = 0;
    }

    void TWindow::setEventCallback( const TEventCallback& p_callback ) { m_data.m_eventCallback = p_callback; }

    GLFWwindow* TWindow::getNativeWindow() const { return m_window; }

    void TWindow::setWindowMode( TWindowMode p_mode )
    {
        if ( p_mode == windowMode() ) return;
        m_pendingWindowMode    = p_mode;
        m_hasPendingWindowMode = true;
    }

    TWindowMode TWindow::windowMode() const { return m_hasPendingWindowMode ? m_pendingWindowMode : m_data.m_windowMode; }

    bool TWindow::flushPendingWindowMode()
    {
        if ( !m_hasPendingWindowMode ) return false;
        m_hasPendingWindowMode = false;
        if ( m_pendingWindowMode == m_data.m_windowMode ) return false;
        applyWindowMode( m_pendingWindowMode );
        return true;
    }

    void TWindow::syncCachedSizes()
    {
        glfwGetWindowSize( m_window, &m_data.m_width, &m_data.m_height );
        glfwGetFramebufferSize( m_window, &m_data.m_fbWidth, &m_data.m_fbHeight );
    }

    void TWindow::storeWindowedGeom()
    {
        glfwGetWindowSize( m_window, &m_windowedW, &m_windowedH );
        glfwGetWindowPos( m_window, &m_windowedX, &m_windowedY );
    }

    void TWindow::applyWindowMode( TWindowMode p_mode )
    {
        if ( m_window == nullptr || p_mode == m_data.m_windowMode ) return;

        GLFWmonitor*       monitor = glfwGetPrimaryMonitor();
        const GLFWvidmode* mode    = monitor != nullptr ? glfwGetVideoMode( monitor ) : nullptr;

        if ( p_mode != TWindowMode::Windowed && ( monitor == nullptr || mode == nullptr ) )
        {
            TLOG_WARN() << "Window mode change requested but no primary monitor/video mode";
            return;
        }

        if ( m_data.m_windowMode == TWindowMode::Windowed ) storeWindowedGeom();

        switch ( p_mode )
        {
            case TWindowMode::Windowed:
            {
                glfwSetWindowAttrib( m_window, GLFW_DECORATED, GLFW_TRUE );
                glfwSetWindowMonitor( m_window, nullptr, m_windowedX, m_windowedY, m_windowedW, m_windowedH, 0 );
                break;
            }
            case TWindowMode::Borderless:
            {
                int mx = 0, my = 0;
                glfwGetMonitorPos( monitor, &mx, &my );
                glfwSetWindowAttrib( m_window, GLFW_DECORATED, GLFW_FALSE );
                glfwSetWindowMonitor( m_window, nullptr, mx, my, mode->width, mode->height, 0 );
                break;
            }
            case TWindowMode::Exclusive:
            {
                glfwSetWindowAttrib( m_window, GLFW_DECORATED, GLFW_TRUE );
                glfwSetWindowMonitor( m_window, monitor, 0, 0, mode->width, mode->height, mode->refreshRate );
                break;
            }
        }

        m_data.m_windowMode = p_mode;
        syncCachedSizes();
    }
}  // namespace Tomos
