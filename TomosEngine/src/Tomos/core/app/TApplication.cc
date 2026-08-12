#include "Tomos/core/app/TApplication.hh"

#include <stdexcept>

#include "Tomos/core/events/TEvent.hh"
#include "Tomos/core/events/application/TApplicationEvent.hh"
#include "Tomos/gpu/vulkan/TVkGpu.hh"
#include "Tomos/gpu/vulkan/renderer/TVkClusteredRenderer.hh"
#include "Tomos/util/logger/TLogger.hh"
#include "Tomos/util/path/TPath.hh"

namespace Tomos
{
    TApplication* TApplication::s_instance = nullptr;

    TApplication::TApplication( const TWindowProps& p_props, const std::string& p_configPath )
    {
        if ( s_instance != nullptr ) throw std::runtime_error( "[TApplication] Only one application instance is allowed" );

        s_instance = this;

        TPath::init( p_configPath );
        const std::string resolvedConfig = TPath::resolveString( p_configPath );

        auto& cfg = m_configManager.load<TEngineConfig>( resolvedConfig );
        // Constructor props seed the window when no config file exists yet.
        // A saved tomos.json wins on subsequent launches.
        if ( !m_configManager.loadedFromFile() )
        {
            cfg.windowTitle  = p_props.m_title;
            cfg.windowWidth  = p_props.m_width;
            cfg.windowHeight = p_props.m_height;
            cfg.vsync        = p_props.m_vsync;
        }

        const TWindowProps winProps{ static_cast<const std::string&>( cfg.windowTitle ), cfg.windowWidth, cfg.windowHeight, cfg.vsync, p_props.m_aspectRatio };

        m_window = std::make_unique<TWindow>( winProps );
        m_window->setEventCallback( [ this ]( TEvent& p_event ) { onEvent( p_event ); } );
        if ( cfg.fullscreen )
        {
            m_window->setFullscreen( true );
            m_window->flushPendingFullscreen();  // apply before initGpu / first frame
        }
    }

    TApplication::~TApplication()
    {
        // Detach layers before clearing assets — onDetach/deactivate still
        // borrow mesh/material/clip pointers. clear() calls onDetach.
        if ( m_gpu ) m_gpu->waitIdle();
        m_layerStack.clear();
        m_assetSystem.clear();
        // Bag TVkImages (and node graph) must die before vkDestroyDevice —
        // m_sceneManager outlives m_gpu in member order.
        m_sceneManager.shutdown();
        m_gpu.reset();
        s_instance = nullptr;
    }

    void TApplication::initGpu( bool p_validation )
    {
        m_gpu = std::make_unique<TVkGpu>( m_window->getNativeWindow(), p_validation );
#ifdef TOMOS_DEBUG
        m_shaderHotReload.init();
#endif
    }

    void TApplication::pushLayer( std::unique_ptr<TLayer> p_layer ) { m_layerStack.pushLayer( std::move( p_layer ) ); }

    void TApplication::pushOverlay( std::unique_ptr<TLayer> p_overlay ) { m_layerStack.pushOverlay( std::move( p_overlay ) ); }

    void TApplication::run()
    {
        while ( m_running )
        {
            m_time.tick();
            const float dt = m_time.dt();

#ifdef TOMOS_DEBUG
            m_window->updatePerfStats( m_time.realDt() );
#endif

            if ( m_window->flushPendingFullscreen() && m_gpu ) m_gpu->noteSurfaceResized();

            m_sceneManager.switchPoint();

            for ( auto& layer : m_layerStack ) layer->onUpdate( dt );

            if ( m_gpu )
            {
#ifdef TOMOS_DEBUG
                if ( m_shaderHotReload.tick( m_window->getNativeWindow() ) )
                {
                    m_gpu->waitIdle();
                    if ( auto* renderer = m_gpu->renderer() ) renderer->reloadShaders();
                }
#endif
                m_gpu->startFrame();

                for ( auto& layer : m_layerStack ) layer->onRender();

                m_gpu->endFrame();
            }

            m_window->onUpdate();
        }
    }

    void TApplication::onEvent( TEvent& p_event )
    {
        TEventDispatcher dispatcher( p_event );
        dispatcher.dispatch<TWindowCloseEvent>( [ this ]( TWindowCloseEvent& e ) { return onWindowClose( e ); } );
        dispatcher.dispatch<TWindowResizeEvent>( [ this ]( TWindowResizeEvent& e ) { return onWindowResize( e ); } );

        for ( auto it = m_layerStack.rbegin(); it != m_layerStack.rend(); ++it )
        {
            if ( p_event.isHandled() ) break;
            ( *it )->onEvent( p_event );
        }
    }

    bool TApplication::onWindowClose( TWindowCloseEvent& )
    {
        close();
        return true;
    }

    bool TApplication::onWindowResize( TWindowResizeEvent& p_event )
    {
        if ( p_event.getWidth() == 0 || p_event.getHeight() == 0 )
        {
            TLOG_WARN() << "Window minimised";
            return false;
        }
        if ( m_gpu ) m_gpu->noteSurfaceResized();
        return false;
    }
}  // namespace Tomos
