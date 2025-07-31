#pragma once

#include "Tomos/util/renderer/TBuffer.hh"
#include "Tomos/util/renderer/TShader.hh"
#include "Tomos/util/renderer/TVertexArray.hh"

namespace Tomos
{
    class TMesh
    {
    public:
        TMesh( const std::shared_ptr<TVertexBuffer>& p_position,
              const std::shared_ptr<TVertexBuffer>& p_normal,
              const std::shared_ptr<TVertexBuffer>& p_texCoord,
              const std::shared_ptr<TVertexBuffer>& p_tangent,
              const std::shared_ptr<TIndexBuffer>&  p_index,
              const std::shared_ptr<TShader>&       p_shader
                );

        ~TMesh() = default;

        const std::shared_ptr<TVertexArray>& getVertexArray() const { return m_vertexArray; }
        const std::shared_ptr<TIndexBuffer>& getIndexBuffer() const { return m_index; }
        const std::shared_ptr<TShader>&      getShader() const { return m_shader; }

    private:
        void setupVertexAttributes() const;

        std::shared_ptr<TVertexArray>  m_vertexArray;
        std::shared_ptr<TVertexBuffer> m_position;
        std::shared_ptr<TVertexBuffer> m_normal;
        std::shared_ptr<TVertexBuffer> m_texCoord;
        std::shared_ptr<TVertexBuffer> m_tangent;
        std::shared_ptr<TIndexBuffer>  m_index;
        std::shared_ptr<TShader>       m_shader;
    };
} // namespace Tomos
