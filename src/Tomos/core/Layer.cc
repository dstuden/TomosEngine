//
// Created by dstuden on 1/22/25.
//

#include "Layer.hh"

#include "Application.hh"
#include "Tomos/util/logger/Logger.hh"

namespace Tomos
{
    Layer::Layer( const std::string& p_id ) : m_layerId( p_id )
    {
        auto& window       = Application::get()->getWindow();
        m_sceneManager     = SceneManager();
        m_layerFramebuffer = std::make_shared<LayerFrameBuffer>( window.getData().m_width, window.getData().m_height );
    }

    LayerStack::LayerStack() {}

    LayerStack::~LayerStack()
    {
        for ( Layer* layer : m_layers )
        {
            layer->onDetach();
            delete layer;
        }
    }

    void LayerStack::pushLayer( Layer* p_layer )
    {
        m_layers.emplace( m_layers.begin() + m_layerInsertIndex, p_layer );
        m_layerInsertIndex++;
    }

    void LayerStack::pushOverlay( Layer* p_overlay ) { m_layers.emplace_back( p_overlay ); }

    void LayerStack::popLayer( Layer* p_layer )
    {
        LOG_DEBUG() << "Start";
        auto it = std::find( m_layers.begin(), m_layers.begin() + m_layerInsertIndex, p_layer );
        if ( it != m_layers.begin() + m_layerInsertIndex )
        {
            p_layer->onDetach();
            m_layers.erase( it );
            m_layerInsertIndex--;
        }
        LOG_DEBUG() << "End";
    }

    void LayerStack::popOverlay( Layer* p_overlay )
    {
        LOG_DEBUG() << "Start";
        auto it = std::find( m_layers.begin() + m_layerInsertIndex, m_layers.end(), p_overlay );
        if ( it != m_layers.end() )
        {
            p_overlay->onDetach();
            m_layers.erase( it );
        }
        LOG_DEBUG() << "End";
    }

    void Layer::addRenderPass( std::shared_ptr<RenderPass> p_pass ) { m_passes.push_back( p_pass ); }

    const std::vector<std::shared_ptr<RenderPass>>& Layer::getRenderPasses() const { return m_passes; }

    void Layer::onResize( int p_width, int p_height )
    {
        m_layerFramebuffer->resize( p_width, p_height );
        for ( auto& pass : m_passes )
        {
            pass->resize( p_width, p_height );
        }
    }
}  // namespace Tomos
