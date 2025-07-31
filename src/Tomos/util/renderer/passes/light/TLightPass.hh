#pragma once
#include "Tomos/core/TLayer.hh"
#include "Tomos/util/renderer/TFrameBuffer.hh"
#include "Tomos/util/renderer/passes/TRenderPass.hh"
#include "Tomos/util/renderer/passes/gbuff/TGBufferPass.hh"

namespace Tomos
{

    class TLightPass : public TRenderPass
    {
    public:
        TLightPass( const std::shared_ptr<TGBufferPass>& p_gBufferPass, const std::string& p_name, const std::shared_ptr<TLayer>& p_layer );

        void resize( unsigned int p_width, unsigned int p_height );
        void execute();

    private:
        std::shared_ptr<TGBufferPass> m_gBufferPass;
        std::shared_ptr<TLayer>       m_layer;
    };

}  // namespace Tomos
