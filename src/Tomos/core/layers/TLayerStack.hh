#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

#include "Tomos/core/layers/TLayer.hh"

namespace Tomos
{
    // Update bottom→top; events top→bottom. Layers below m_layerInsertIdx; overlays above.
    class TLayerStack
    {
    public:
        TLayerStack()  = default;
        ~TLayerStack() = default;

        TLayerStack( const TLayerStack& )            = delete;
        TLayerStack& operator=( const TLayerStack& ) = delete;

        void pushLayer( std::unique_ptr<TLayer> p_layer );

        void pushOverlay( std::unique_ptr<TLayer> p_overlay );

        [[nodiscard]] std::unique_ptr<TLayer> popLayer( TLayer* p_layer );

        [[nodiscard]] std::unique_ptr<TLayer> popOverlay( TLayer* p_overlay );

        // Call while GPU is still alive.
        void clear();

        [[nodiscard]] auto begin() { return m_layers.begin(); }
        [[nodiscard]] auto end() { return m_layers.end(); }
        [[nodiscard]] auto rbegin() { return m_layers.rbegin(); }
        [[nodiscard]] auto rend() { return m_layers.rend(); }

        // Layers below insert index; overlays from insert index to end.
        [[nodiscard]] auto layersBegin() { return m_layers.begin(); }
        [[nodiscard]] auto layersEnd() { return m_layers.begin() + static_cast<std::ptrdiff_t>( m_layerInsertIdx ); }
        [[nodiscard]] auto overlaysBegin() { return m_layers.begin() + static_cast<std::ptrdiff_t>( m_layerInsertIdx ); }
        [[nodiscard]] auto overlaysEnd() { return m_layers.end(); }

        [[nodiscard]] bool   empty() const { return m_layers.empty(); }
        [[nodiscard]] size_t size() const { return m_layers.size(); }

    private:
        std::vector<std::unique_ptr<TLayer>> m_layers;
        uint32_t                             m_layerInsertIdx = 0;
    };
}  // namespace Tomos
