#include "Tomos/core/layers/TLayerStack.hh"

#include <algorithm>
#include <ranges>

namespace Tomos
{
    void TLayerStack::pushLayer( std::unique_ptr<TLayer> p_layer )
    {
        p_layer->onAttach();
        m_layers.insert( m_layers.begin() + m_layerInsertIdx, std::move( p_layer ) );
        ++m_layerInsertIdx;
    }

    void TLayerStack::pushOverlay( std::unique_ptr<TLayer> p_overlay )
    {
        p_overlay->onAttach();
        m_layers.push_back( std::move( p_overlay ) );
    }

    std::unique_ptr<TLayer> TLayerStack::popLayer( TLayer* p_layer )
    {
        auto it = std::find_if( m_layers.begin(), m_layers.begin() + m_layerInsertIdx, [ p_layer ]( const auto& p_l ) { return p_l.get() == p_layer; } );

        if ( it == m_layers.begin() + m_layerInsertIdx ) return nullptr;

        ( *it )->onDetach();
        auto owned = std::move( *it );
        m_layers.erase( it );
        --m_layerInsertIdx;
        return owned;
    }

    std::unique_ptr<TLayer> TLayerStack::popOverlay( TLayer* p_overlay )
    {
        auto it = std::find_if( m_layers.begin() + m_layerInsertIdx, m_layers.end(), [ p_overlay ]( const auto& p_l ) { return p_l.get() == p_overlay; } );

        if ( it == m_layers.end() ) return nullptr;

        ( *it )->onDetach();
        auto owned = std::move( *it );
        m_layers.erase( it );
        return owned;
    }

    void TLayerStack::clear()
    {
        for ( auto& m_layer : std::views::reverse( m_layers ) ) m_layer->onDetach();
        m_layers.clear();
        m_layerInsertIdx = 0;
    }
}  // namespace Tomos
