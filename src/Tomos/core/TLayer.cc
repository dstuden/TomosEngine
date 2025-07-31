//
// Created by dstuden on 1/22/25.
//

#include "TApplication.hh"
#include "TLayer.hh"
#include "Tomos/util/logger/TLogger.hh"

namespace Tomos
{
    TLayer::TLayer( const std::string& p_id ) : m_layerId( p_id )
    {
        auto& window       = TApplication::get()->getWindow();
        m_sceneManager     = SceneManager();
        m_layerFramebuffer = std::make_shared<LayerFrameBuffer>( window.getData().m_width, window.getData().m_height );
    }

    LayerStack::LayerStack() {}

    LayerStack::~LayerStack()
    {
        for ( TLayer* layer : m_layers )
        {
            layer->onDetach();
            delete layer;
        }
    }

    void LayerStack::pushLayer( TLayer* p_layer )
    {
        m_layers.emplace( m_layers.begin() + m_layerInsertIndex, p_layer );
        m_layerInsertIndex++;
    }

    void LayerStack::pushOverlay( TLayer* p_overlay ) { m_layers.emplace_back( p_overlay ); }

    void LayerStack::popLayer( TLayer* p_layer )
    {
        TLOG_DEBUG() << "Start";
        auto it = std::find( m_layers.begin(), m_layers.begin() + m_layerInsertIndex, p_layer );
        if ( it != m_layers.begin() + m_layerInsertIndex )
        {
            p_layer->onDetach();
            m_layers.erase( it );
            m_layerInsertIndex--;
        }
        TLOG_DEBUG() << "End";
    }

    void LayerStack::popOverlay( TLayer* p_overlay )
    {
        TLOG_DEBUG() << "Start";
        auto it = std::find( m_layers.begin() + m_layerInsertIndex, m_layers.end(), p_overlay );
        if ( it != m_layers.end() )
        {
            p_overlay->onDetach();
            m_layers.erase( it );
        }
        TLOG_DEBUG() << "End";
    }

    void TLayer::addRenderPass( std::shared_ptr<TRenderPass> p_pass ) { m_passes.push_back( p_pass ); }

    const std::vector<std::shared_ptr<TRenderPass>>& TLayer::getRenderPasses() const { return m_passes; }

    void TLayer::onResize( int p_width, int p_height )
    {
        m_layerFramebuffer->resize( p_width, p_height );
        for ( auto& pass : m_passes )
        {
            pass->resize( p_width, p_height );
        }
    }
}  // namespace Tomos
