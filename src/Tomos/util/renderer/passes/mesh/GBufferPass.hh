#pragma once

#include "../RenderPass.hh"

namespace Tomos
{
    class GBufferPass : public RenderPass
    {
    public:
        GBufferPass( unsigned int p_width, unsigned int p_height, const std::string& p_layerId );
        void                         resize( unsigned int p_width, unsigned int p_height );
        void                         execute();

    private:
    };

}  // namespace Tomos
