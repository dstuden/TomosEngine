#pragma once

#include <memory>
#include <string>

#include "../FrameBuffer.hh"

namespace Tomos
{
    class RenderPass
    {
    public:
        RenderPass( const std::string& p_name, const std::string& p_layerId ) : m_name( p_name ), m_layerId( p_layerId ) {}

        virtual ~RenderPass() = default;

        virtual void resize( unsigned int p_width, unsigned int p_height ) = 0;
        virtual void execute()                                             = 0;

        const std::string&           getName() const { return m_name; }
        std::shared_ptr<FrameBuffer> getFrameBuffer() const { return m_frameBuffer; }

    protected:
        std::string                  m_name;
        std::shared_ptr<FrameBuffer> m_frameBuffer;
        const std::string&           m_layerId;
    };
}  // namespace Tomos
