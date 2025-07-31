#pragma once

#include <vector>
#include <string>

#include <GL/glew.h>

namespace Tomos
{
    enum class ShaderDataType
    {
        None = 0,
        Float,
        Float2,
        Float3,
        Float4,
        Mat3,
        Mat4,
        Int,
        Int2,
        Int3,
        Int4,
        Bool
    };

    static GLenum shaderDataTypeToOpenGLBaseType( ShaderDataType p_type )
    {
        switch ( p_type )
        {
            case ShaderDataType::Float:
            case ShaderDataType::Float2:
            case ShaderDataType::Float3:
            case ShaderDataType::Float4:
            case ShaderDataType::Mat3:
            case ShaderDataType::Mat4:
                return GL_FLOAT;
            case ShaderDataType::Int:
            case ShaderDataType::Int2:
            case ShaderDataType::Int3:
            case ShaderDataType::Int4:
                return GL_INT;
            case ShaderDataType::Bool:
                return GL_BOOL;
            case ShaderDataType::None:
            default:
                return 0;
        }
    }

    static unsigned int shaderDataTypeSize( ShaderDataType p_type )
    {
        switch ( p_type )
        {
            case ShaderDataType::Float:
                return 4;
            case ShaderDataType::Float2:
                return 4 * 2;
            case ShaderDataType::Float3:
                return 4 * 3;
            case ShaderDataType::Float4:
                return 4 * 4;
            case ShaderDataType::Mat3:
                return 4 * 3 * 3;
            case ShaderDataType::Mat4:
                return 4 * 4 * 4;
            case ShaderDataType::Int:
                return 4;
            case ShaderDataType::Int2:
                return 4 * 2;
            case ShaderDataType::Int3:
                return 4 * 3;
            case ShaderDataType::Int4:
                return 4 * 4;
            case ShaderDataType::Bool:
                return 1;
            case ShaderDataType::None:
            default:
                return 0;
        }
    }

    class TBufferElement
    {
    public:
        std::string    m_name;
        unsigned int   m_offset;
        unsigned int   m_size;
        ShaderDataType m_type;
        bool           m_normalized;

        TBufferElement() = default;

        TBufferElement( ShaderDataType p_type, const std::string& p_name, bool p_normalized = false ) :
            m_name( p_name ),
            m_offset( 0 ),
            m_size( shaderDataTypeSize( p_type ) ),
            m_type( p_type ),
            m_normalized( p_normalized )
        {
        }

        unsigned int getComponentCount() const
        {
            switch ( m_type )
            {
                case ShaderDataType::Float:
                    return 1;
                case ShaderDataType::Float2:
                    return 2;
                case ShaderDataType::Float3:
                    return 3;
                case ShaderDataType::Float4:
                    return 4;
                case ShaderDataType::Mat3:
                    return 3 * 3;
                case ShaderDataType::Mat4:
                    return 4 * 4;
                case ShaderDataType::Int:
                    return 1;
                case ShaderDataType::Int2:
                    return 2;
                case ShaderDataType::Int3:
                    return 3;
                case ShaderDataType::Int4:
                    return 4;
                case ShaderDataType::Bool:
                    return 1;
                case ShaderDataType::None:
                default:
                    return 0;
            }
        }
    };

    class TBufferLayout
    {
    public:
        TBufferLayout() = default;

        TBufferLayout( const std::initializer_list<TBufferElement>& p_elements ) :
            m_elements( p_elements )
        {
            calculateOffsetsAndStride();
        }

        inline const std::vector<TBufferElement>& getElements() const { return m_elements; }
        inline unsigned int                      getStride() const { return m_stride; }

        std::vector<TBufferElement>::iterator begin() { return m_elements.begin(); }
        std::vector<TBufferElement>::iterator end() { return m_elements.end(); }

    private:
        void calculateOffsetsAndStride();

        std::vector<TBufferElement> m_elements;
        unsigned int               m_stride = 0;
    };

    class TVertexBuffer
    {
    public:
        void bind() const;
        void unbind() const;

        void                setLayout( const TBufferLayout& p_layout ) { m_layout = p_layout; }
        const TBufferLayout& getLayout() const { return m_layout; }
        unsigned int        getSize() const { return m_size; }

        TVertexBuffer( const float* p_verticies, unsigned int p_size, GLenum p_usage = GL_STATIC_DRAW );
        ~TVertexBuffer();

    private:
        unsigned int m_rendererId{};
        TBufferLayout m_layout;
        unsigned int m_size{};
    };

    class TIndexBuffer
    {
    public:
        void bind() const;
        void unbind() const;

        TIndexBuffer( const unsigned int* p_indicies, unsigned int p_count, GLenum p_usage = GL_STATIC_DRAW );
        ~TIndexBuffer();

        unsigned int getCount() const { return m_count; }

    private:
        unsigned int m_rendererId{};
        unsigned int m_count;
    };

    class TStorageBuffer
    {
    public:
        TStorageBuffer( unsigned int p_size, GLenum p_usage = GL_DYNAMIC_DRAW );
        ~TStorageBuffer();

        void bind() const;
        void unbind() const;
        void bindBase( unsigned int p_bindingPoint ) const;

        void  setData( const void* p_data, unsigned int p_size, unsigned int p_offset = 0 );
        void* map( GLenum p_access = GL_WRITE_ONLY );
        void  unmap();

        unsigned int getSize() const { return m_size; }

    private:
        unsigned int m_rendererId{};
        unsigned int m_size{};
    };
} // Tomos
