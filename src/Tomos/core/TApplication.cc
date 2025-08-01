//
// Created by dstuden on 1/19/25.
//

#include "TApplication.hh"

#include <glm/glm.hpp>

#include "Tomos/events/application/TApplicationEvent.hh"
#include "Tomos/systems/camera/TCameraSystem.hh"
#include "Tomos/util/logger/TLogger.hh"
#include "Tomos/util/renderer/TBuffer.hh"
#include "Tomos/util/renderer/TRenderer.hh"

namespace Tomos
{
    TApplication* TApplication::g_instance = nullptr;

    TApplication::TApplication( const WindowProps& p_props )
    {
        TLOG_INFO() << "Tomos Engine";
        TLOG_INFO() << "By dstuden";
        TLOG_INFO() << "Initializing Application";

        m_window = std::make_unique<TWindow>( p_props );

        m_window->setEventCallback( [this]( TEvent& e ) { onEvent( e ); } );
    }

    TApplication* TApplication::get()
    {
        TLOG_ASSERT_MSG( g_instance, "Application is not initialized!" );

        return g_instance;
    }

    void TApplication::cleanup()
    {
        if ( g_instance )
        {
            TLOG_INFO() << "Cleaning up Application";
            g_instance->m_window.reset();
            delete g_instance;
            g_instance = nullptr;
        }
        else
        {
            TLOG_ERROR() << "Application is not initialized!";
        }
    }

    void TApplication::init( const WindowProps& p_props )
    {
        if ( g_instance == nullptr )
        {
            g_instance = new TApplication( p_props );
        }
        else
        {
            TLOG_ERROR() << "Application already exists!";
        }
    }

    void TApplication::run()
    {
        TLOG_DEBUG() << "Start";

        while ( m_running )
        {
            // Time update
            auto currentTime = ( float ) glfwGetTime();
            getState().time().update( currentTime );

            getState().ecs().updateLayerComponents();

            for ( auto& layer : getState().layerStack() )
            {
                // Layer specific ECS update
                getState().ecs().earlyUpdate( layer->getLayerId() );
                getState().ecs().update( layer->getLayerId() );

                // Layer specific fixed time step ECS update
                getState().ecs().updateFixedTimeStep( getState().time().deltaTime(), layer->getLayerId() );

                // Layer specific update
                layer->onUpdate();

                // Layer specific late ECS update
                getState().ecs().lateUpdate( layer->getLayerId() );

                for ( auto& pass : layer->getRenderPasses() )
                {
                    pass->execute();
                }
            }

            // Second pass: Composite all layers
            TRenderer::clearFrameBuffer();  // To the screen
            TRenderer::setClearedColor( glm::vec4( 0.0f, 0.0f, 0.0f, 1.0f ) );
            TRenderer::clear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );

            for ( auto& layer : getState().layerStack() )
            {
                TRenderer::renderLayerFrameBufferToQuad( layer->getLayerFramebuffer(), layer->getQuad(), layer->getShader() );
            }

            // Window update
            m_window->onUpdate();
        }
        TLOG_DEBUG() << "End";
    }

    void TApplication::onEvent( TEvent& p_e )
    {
        TLOG_DEBUG() << p_e.toString();

        EventDispatcher dispatcher( p_e );
        dispatcher.dispatch<WindowCloseEvent>( [this]( TEvent& p_event ) { return onWindowClose( dynamic_cast<WindowCloseEvent&>( p_event ) ); } );

        dispatcher.dispatch<WindowResizeEvent>(
                [this]( TEvent& p_event )
                {
                    auto& e = dynamic_cast<WindowResizeEvent&>( p_event );
                    int   viewportWidth, viewportHeight;
                    int   offsetX = 0, offsetY = 0;

                    if ( ( float ) e.getWidth() / ( float ) e.getHeight() > getWindow().getData().m_aspectRatio )
                    {
                        viewportHeight = e.getHeight();
                        viewportWidth  = ( int ) ( e.getHeight() * getWindow().getData().m_aspectRatio );
                        offsetX        = ( e.getWidth() - viewportWidth ) / 2;
                    }
                    else
                    {
                        viewportWidth  = e.getWidth();
                        viewportHeight = ( int ) ( e.getWidth() / getWindow().getData().m_aspectRatio );
                        offsetY        = ( e.getHeight() - viewportHeight ) / 2;
                    }

                    // Resize all framebuffers
                    for ( auto& layer : getState().layerStack() )
                    {
                        layer->onResize( e.getWidth(), e.getHeight() );
                    }

                    glViewport( offsetX, offsetY, viewportWidth, viewportHeight );
                    return false;
                } );

        for ( auto it = getState().layerStack().end(); it != getState().layerStack().begin(); )
        {
            ( *--it )->onEvent( p_e );
            if ( p_e.isHandled() )
            {
                break;
            }
        }
    }

    bool TApplication::onWindowClose( WindowCloseEvent& p_e )
    {
        m_running = false;
        return true;
    }

    TWindow& TApplication::getWindow() const { return *m_window; }
}  // namespace Tomos
