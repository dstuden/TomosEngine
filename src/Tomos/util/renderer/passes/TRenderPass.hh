#pragma once

#include <memory>
#include <string>

#include "../TFrameBuffer.hh"

namespace Tomos
{
    class TRenderPass
    {
    public:
        TRenderPass( const std::string& p_name, const std::string& p_layerId ) : m_name( p_name ), m_layerId( p_layerId ) {}

        virtual ~TRenderPass() = default;

        virtual void resize( unsigned int p_width, unsigned int p_height ) = 0;
        virtual void execute()                                             = 0;

        const std::string&            getName() const { return m_name; }
        std::shared_ptr<TFrameBuffer> getFrameBuffer() const { return m_frameBuffer; }

    protected:
        std::string                   m_name;
        std::shared_ptr<TFrameBuffer> m_frameBuffer;
        const std::string&            m_layerId;
    };
}  // namespace Tomos
