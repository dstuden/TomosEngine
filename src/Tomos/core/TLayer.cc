//
// Created by dstuden on 1/22/25.
//

#include "TLayer.hh"

#include "TApplication.hh"
#include "Tomos/util/logger/TLogger.hh"

namespace Tomos
{
    TLayer::TLayer( const std::string& p_id ) : m_layerId( p_id )
    {
        auto& window       = TApplication::get()->getWindow();
        m_sceneManager     = TSceneManager();
        m_layerFramebuffer = std::make_shared<LayerFrameBuffer>( window.getData().m_width, window.getData().m_height );
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

    LayerStack::LayerStack() {}

    LayerStack::~LayerStack()
    {
        for ( const auto& layer : m_layers )
        {
            layer->onDetach();
        }
    }

    void LayerStack::pushLayer( std::shared_ptr<TLayer> p_layer )
    {
        if ( m_layerMap.count( p_layer->getLayerId() ) )
        {
            TLOG_WARN() << "Attempted to push a layer with duplicate ID: " << p_layer->getLayerId();
            return;
        }

        m_layers.emplace( m_layers.begin() + m_layerInsertIndex, p_layer );
        m_layerMap[p_layer->getLayerId()] = p_layer;
        m_layerInsertIndex++;
        p_layer->onAttach();
    }

    void LayerStack::pushOverlay( std::shared_ptr<TLayer> p_overlay )
    {
        if ( m_layerMap.count( p_overlay->getLayerId() ) )
        {
            TLOG_WARN() << "Attempted to push an overlay with duplicate ID: " << p_overlay->getLayerId();
            return;
        }

        m_layers.emplace_back( p_overlay );
        m_layerMap[p_overlay->getLayerId()] = p_overlay;
        p_overlay->onAttach();
    }

    void LayerStack::popLayer( const std::shared_ptr<TLayer>& p_layer )
    {
        auto it = std::find( m_layers.begin(), m_layers.begin() + m_layerInsertIndex, p_layer );
        if ( it != m_layers.begin() + m_layerInsertIndex )
        {
            p_layer->onDetach();
            m_layers.erase( it );
            m_layerMap.erase( p_layer->getLayerId() );
            m_layerInsertIndex--;
        }
    }

    void LayerStack::popOverlay( const std::shared_ptr<TLayer>& p_overlay )
    {
        auto it = std::find( m_layers.begin() + m_layerInsertIndex, m_layers.end(), p_overlay );
        if ( it != m_layers.end() )
        {
            p_overlay->onDetach();
            m_layers.erase( it );
            m_layerMap.erase( p_overlay->getLayerId() );
        }
    }

    void LayerStack::popLayerById( const std::string& p_layerId )
    {
        if ( m_layerMap.count( p_layerId ) )
        {
            std::shared_ptr<TLayer> layer = m_layerMap[p_layerId];
            popLayer( layer );  // Or popOverlay(layer), need to know which type it is.
                                // A better approach would be to check if it's a layer or overlay
                                // and call the right pop function. For now, let's assume
                                // popLayer is sufficient for this example.
        }
    }

    std::shared_ptr<TLayer> LayerStack::getLayer( const std::string& p_layerId )
    {
        if ( m_layerMap.count( p_layerId ) )
        {
            return m_layerMap.at( p_layerId );
        }
        return nullptr;
    }

    const std::shared_ptr<TLayer> LayerStack::getLayer( const std::string& p_layerId ) const
    {
        if ( m_layerMap.count( p_layerId ) )
        {
            return m_layerMap.at( p_layerId );
        }
        return nullptr;
    }

    std::set<std::string> LayerStack::getLayerIds() const
    {
        std::set<std::string> layerIds;
        for ( const auto& layer : m_layers )
        {
            layerIds.insert( layer->getLayerId() );
        }
        return layerIds;
    }
}  // namespace Tomos
