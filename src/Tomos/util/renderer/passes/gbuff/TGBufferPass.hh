#pragma once

#include "../TRenderPass.hh"

namespace Tomos
{
    class TGBufferPass : public TRenderPass
    {
    public:
        TGBufferPass( unsigned int p_width, unsigned int p_height, const std::string& p_layerId );
        void                         resize( unsigned int p_width, unsigned int p_height );
        void                         execute();

    private:
    };

}  // namespace Tomos
