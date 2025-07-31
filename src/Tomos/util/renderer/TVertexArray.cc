//
// Created by dstuden on 2/20/25.
//

#include <GL/glew.h>
#include <glm/fwd.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>

#include "../logger/TLogger.hh"
#include "TVertexArray.hh"

namespace Tomos
{
    void TVertexArray::bind() const
    {
        glBindVertexArray( m_rendererId );
    }

    void TVertexArray::unbind() const
    {
        glBindVertexArray( 0 );
    }

    void TVertexArray::addVertexBuffer( const std::shared_ptr<TVertexBuffer>& p_vertexBuffer )
    {
        TLOG_DEBUG() << "Start";

        TLOG_ASSERT( !p_vertexBuffer->getLayout().getElements().empty() );

        bind();
        p_vertexBuffer->bind();

        auto layout = p_vertexBuffer->getLayout();
        for ( const auto& element : layout )
        {
            switch ( element.m_type )
            {
                case ShaderDataType::Float:
                case ShaderDataType::Float2:
                case ShaderDataType::Float3:
                case ShaderDataType::Float4:
                {
                    glEnableVertexAttribArray( m_vertexBufferIndex );
                    glVertexAttribPointer( m_vertexBufferIndex,
                                           element.getComponentCount(),
                                           shaderDataTypeToOpenGLBaseType( element.m_type ),
                                           element.m_normalized ? GL_TRUE : GL_FALSE,
                                           layout.getStride(),
                                           ( const void* ) element.m_offset );

                    TLOG_DEBUG() << "Vertex buffer index: " << m_vertexBufferIndex
                            << " Element: " << element.m_name
                            << " Offset: " << element.m_offset
                            << " Size: " << element.m_size
                            << " Stride: " << layout.getStride()
                            << " Type: " << shaderDataTypeToOpenGLBaseType( element.m_type );
                    m_vertexBufferIndex++;
                    break;
                }
                case ShaderDataType::Int:
                case ShaderDataType::Int2:
                case ShaderDataType::Int3:
                case ShaderDataType::Int4:
                case ShaderDataType::Bool:
                {
                    glEnableVertexAttribArray( m_vertexBufferIndex );
                    glVertexAttribIPointer( m_vertexBufferIndex,
                                            element.getComponentCount(),
                                            shaderDataTypeToOpenGLBaseType( element.m_type ),
                                            layout.getStride(),
                                            ( const void* ) element.m_offset );

                    TLOG_DEBUG() << "Vertex buffer index: " << m_vertexBufferIndex
                            << " Element: " << element.m_name
                            << " Offset: " << element.m_offset
                            << " Size: " << element.m_size
                            << " Stride: " << layout.getStride()
                            << " Type: " << shaderDataTypeToOpenGLBaseType( element.m_type );
                    m_vertexBufferIndex++;
                    break;
                }
                case ShaderDataType::Mat3:
                case ShaderDataType::Mat4:
                {
                    uint8_t count = element.getComponentCount();
                    for ( uint8_t i = 0; i < count; i++ )
                    {
                        glEnableVertexAttribArray( m_vertexBufferIndex );
                        glVertexAttribPointer( m_vertexBufferIndex,
                                               count,
                                               shaderDataTypeToOpenGLBaseType( element.m_type ),
                                               element.m_normalized ? GL_TRUE : GL_FALSE,
                                               layout.getStride(),
                                               ( const void* ) ( element.m_offset + sizeof( float ) * count * i ) );
                        glVertexAttribDivisor( m_vertexBufferIndex, 1 );

                        TLOG_DEBUG() << "Vertex buffer index: " << m_vertexBufferIndex
                                << " Element: " << element.m_name
                                << " Offset: " << element.m_offset
                                << " Size: " << element.m_size
                                << " Stride: " << layout.getStride()
                                << " Type: " << shaderDataTypeToOpenGLBaseType( element.m_type );
                        m_vertexBufferIndex++;
                    }
                    break;
                }
                default:
                    TLOG_ASSERT_MSG( false, "Unknown ShaderDataType!" );
            }
        }

        m_vertexBuffers.push_back( p_vertexBuffer );

        TLOG_DEBUG() << "End";
    }

    void TVertexArray::setIndexBuffer( const std::shared_ptr<TIndexBuffer>& p_indexBuffer )
    {
        bind();
        p_indexBuffer->bind();

        m_indexBuffer = p_indexBuffer;
    }

    TVertexArray::TVertexArray()
    {
        glCreateVertexArrays( 1, &m_rendererId );
    }

    TVertexArray::~TVertexArray()
    {
    }

    void TVertexArray::setInstanceBuffer( const std::shared_ptr<TStorageBuffer>& p_instanceBuffer, unsigned int p_bindingPoint )
    {
        bind();
        p_instanceBuffer->bindBase( p_bindingPoint );
        m_instanceBuffer = p_instanceBuffer;
    }
} // Tomos
