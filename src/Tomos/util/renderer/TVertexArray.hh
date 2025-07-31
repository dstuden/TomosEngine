#pragma once

#include <memory>
#include <vector>

#include "TBuffer.hh"

namespace Tomos
{
    class TVertexArray
    {
        friend class TRenderer;

    public:
        void bind() const;
        void unbind() const;

        void addVertexBuffer( const std::shared_ptr<TVertexBuffer>& p_vertexBuffer );
        void setIndexBuffer( const std::shared_ptr<TIndexBuffer>& p_indexBuffer );

        const std::vector<std::shared_ptr<TVertexBuffer>>& getVertexBuffers() const { return m_vertexBuffers; }
        const std::shared_ptr<TIndexBuffer>&               getIndexBuffer() const { return m_indexBuffer; }

        TVertexArray();
        ~TVertexArray();

    private:
        void                                  setInstanceBuffer( const std::shared_ptr<TStorageBuffer>& p_instanceBuffer, unsigned int p_bindingPoint );
        const std::shared_ptr<TStorageBuffer>& getInstanceBuffer() const { return m_instanceBuffer; }

        unsigned int                               m_vertexBufferIndex = 0;
        std::vector<std::shared_ptr<TVertexBuffer>> m_vertexBuffers;
        std::shared_ptr<TIndexBuffer>               m_indexBuffer;
        std::shared_ptr<TStorageBuffer>             m_instanceBuffer;

        unsigned int m_rendererId{};
    };
} // Tomos
