#include "Tomos/core/app/TApplication.hh"

#include <ranges>
#include <stdexcept>

#include "Tomos/core/events/TEvent.hh"
#include "Tomos/core/events/application/TApplicationEvent.hh"
#include "Tomos/gpu/vulkan/TVkGpu.hh"
#include "Tomos/gpu/vulkan/renderer/TVkClusteredRenderer.hh"
#include "Tomos/util/logger/TLogger.hh"
#include "Tomos/util/path/TPath.hh"
#include "Tomos/util/profile/TProfile.hh"

#if TOMOS_DEBUG
#include <array>

#include "Tomos/util/profile/TFrameAnalytics.hh"
#endif

namespace Tomos
{
    namespace
    {
#if TOMOS_DEBUG
        TSceneCounts makeSceneCounts( const TFrameState& p_state )
        {
            TSceneCounts c;
            for ( const auto& dc : p_state.m_drawCalls )
            {
                if ( dc.m_visible )
                    ++c.drawCallsForward;
                else
                    ++c.drawCallsShadow;
            }
            c.instances     = p_state.m_instances.size();
            c.lights        = p_state.m_lights.size();
            c.sprites       = p_state.m_sprites.size();
            c.spriteBatches = p_state.m_spriteBatches.size();
            c.emitters      = p_state.m_emitters.size();
            c.bones         = p_state.m_bones.size();
            c.meshesTotal   = p_state.m_meshesTotal;
            c.meshesCulled  = p_state.m_meshesCulled;
            return c;
        }
#endif
    }  // namespace

    TApplication* TApplication::g_sInstance = nullptr;

    TApplication::TApplication( const TWindowProps& p_props, const std::string& p_configPath )
    {
        if ( g_sInstance != nullptr ) throw std::runtime_error( "[TApplication] Only one application instance is allowed" );

        g_sInstance = this;
        TFrameAllocator::setCurrent( &m_frameAllocator );

        TPath::init( p_configPath );
        const std::string resolvedConfig = TPath::configPath().string();

        auto& cfg = m_configManager.load<TEngineConfig>( resolvedConfig );
        // Constructor props seed the window when no config file exists yet.
        // A saved tomos.json wins on subsequent launches.
        if ( !m_configManager.loadedFromFile() )
        {
            cfg.m_windowTitle  = p_props.m_title;
            cfg.m_windowWidth  = p_props.m_width;
            cfg.m_windowHeight = p_props.m_height;
        }

        const TWindowProps winProps{ static_cast<const std::string&>( cfg.m_windowTitle ), cfg.m_windowWidth, cfg.m_windowHeight, p_props.m_aspectRatio };

        m_window = std::make_unique<TWindow>( winProps );
        m_window->setEventCallback( [ this ]( TEvent& p_event ) { onEvent( p_event ); } );

        const TWindowMode mode = TWindow::windowModeFromString( static_cast<const std::string&>( cfg.m_windowMode ) );
        if ( mode != TWindowMode::Windowed )
        {
            m_window->setWindowMode( mode );
            m_window->flushPendingWindowMode();  // apply before initGpu / first frame
        }
    }

    TApplication::~TApplication()
    {
        m_assetLoadQueue.shutdown();
        if ( m_gpu ) m_gpu->waitIdle();
        m_layerStack.clear();
        m_sceneManager.shutdown();
        m_assetSystem.clear();
        m_assetLoadQueue.setGpu( nullptr );
        m_gpu.reset();
        TFrameAllocator::setCurrent( nullptr );
        g_sInstance = nullptr;
    }

    void TApplication::initGpu( bool p_validation )
    {
        m_gpu = std::make_unique<TVkGpu>( m_window->getNativeWindow(), p_validation );
        m_gpu->setSwapchainPresentPolicy( TVkGpu::presentPolicyFromString( static_cast<const std::string&>( config().m_presentMode ) ) );
        m_assetLoadQueue.setGpu( m_gpu.get() );
    }

    void TApplication::pushLayer( std::unique_ptr<TLayer> p_layer ) { m_layerStack.pushLayer( std::move( p_layer ) ); }

    void TApplication::pushOverlay( std::unique_ptr<TLayer> p_overlay ) { m_layerStack.pushOverlay( std::move( p_overlay ) ); }

    void TApplication::run()
    {
        while ( m_running )
        {
            m_frameAllocator.beginFrame();

#if TOMOS_DEBUG
            TFrameProfiler::get().beginFrame();
#endif
            {
                TOMOS_PROFILE_SCOPE( "Frame.Wall" );

                m_time.tick();
                const float dt = m_time.dt();

#if TOMOS_DEBUG
                m_window->updatePerfStats( m_time.realDt() );
#endif

                if ( m_window->flushPendingWindowMode() && m_gpu )
                {
                    const auto& d = m_window->getData();
                    if ( d.m_fbWidth > 0 && d.m_fbHeight > 0 ) m_gpu->noteSurfaceResized();
                }

                m_sceneManager.switchPoint();

                {
                    TOMOS_PROFILE_SCOPE( "App.AssetQueue" );
                    m_assetLoadQueue.tick( m_sceneManager.scene() );
                }

                // Base layers before GPU frame open; overlays after so UI can
                // resize/rebind destinations before render.
                {
                    TOMOS_PROFILE_SCOPE( "App.LayersUpdate" );
                    for ( auto it = m_layerStack.layersBegin(); it != m_layerStack.layersEnd(); ++it )
                    {
                        TOMOS_PROFILE_SCOPE( ( *it )->name().c_str() );
                        ( *it )->onUpdate( dt );
                    }
                }

                {
                    TOMOS_PROFILE_SCOPE( "GPU.StartFrame" );
                    if ( m_gpu ) m_gpu->startFrame();
                }

                {
                    TOMOS_PROFILE_SCOPE( "App.OverlaysUpdate" );
                    for ( auto it = m_layerStack.overlaysBegin(); it != m_layerStack.overlaysEnd(); ++it )
                    {
                        TOMOS_PROFILE_SCOPE( ( *it )->name().c_str() );
                        ( *it )->onUpdate( dt );
                    }
                }

                if ( m_gpu )
                {
                    {
                        TOMOS_PROFILE_SCOPE( "App.Render" );
                        for ( auto& layer : m_layerStack )
                        {
                            TOMOS_PROFILE_SCOPE( layer->name().c_str() );
                            layer->onRender();
                        }
                    }

                    {
                        TOMOS_PROFILE_SCOPE( "GPU.EndFrame" );
                        m_gpu->endFrame();
                    }
                }

                m_window->onUpdate();
            }

#if TOMOS_DEBUG
            TFrameProfiler::get().endFrame();
#endif

            m_frameAllocator.endFrame();

#if TOMOS_DEBUG
            if ( TFrameProfiler::get().isCaptureEnabled() )
            {
                TSceneCounts sceneCounts{};
                const std::array<float, g_kGpuPassCount>* gpuMs = nullptr;
                bool                                      gpuOk = false;
                if ( m_gpu )
                {
                    sceneCounts = makeSceneCounts( m_gpu->frameState() );
                    if ( auto* r = m_gpu->renderer() )
                    {
                        gpuMs = &r->gpuPassMs();
                        gpuOk = r->gpuPassMsValid();
                    }
                }
                TFrameAnalytics::get().capture( m_time.realDt(), m_time.dt(), sceneCounts, m_frameAllocator.stats(), gpuMs, gpuOk );
            }
#endif
        }
    }

    void TApplication::onEvent( TEvent& p_event )
    {
        TEventDispatcher dispatcher( p_event );
        dispatcher.dispatch<TWindowCloseEvent>( [ this ]( TWindowCloseEvent& p_e ) { return onWindowClose( p_e ); } );
        dispatcher.dispatch<TWindowResizeEvent>( [ this ]( TWindowResizeEvent& p_e ) { return onWindowResize( p_e ); } );

        for ( auto& it : std::views::reverse( m_layerStack ) )
        {
            if ( p_event.isHandled() ) break;
            it->onEvent( p_event );
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
