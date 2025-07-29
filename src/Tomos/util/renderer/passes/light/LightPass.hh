#pragma once

#include "../RenderPass.hh"
#include "Tomos/util/renderer/FrameBuffer.hh"
#include "Tomos/core/Layer.hh"

namespace Tomos
{
    class LightPass : public RenderPass
    {
    public:
        LightPass( const std::shared_ptr<FrameBuffer>& p_gbuffer, const std::shared_ptr<Layer>& p_layer );
        void resize( unsigned int p_width, unsigned int p_height );
        void execute();

    protected:
        std::shared_ptr<FrameBuffer> m_gbuffer;
        std::shared_ptr<Layer> m_layer;
    };

}  // namespace Tomos
