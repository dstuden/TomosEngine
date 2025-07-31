//
// Created by dstuden on 4/2/25.
//

#include <GL/glew.h>

#include "TTexture.hh"
#include "Tomos/lib/stb_image.h"
#include "Tomos/util/logger/TLogger.hh"

namespace Tomos
{

    std::shared_ptr<TTexture> TTexture::createWhiteTexture()
    {
        GLuint textureID;
        glGenTextures( 1, &textureID );
        glBindTexture( GL_TEXTURE_2D, textureID );

        unsigned char whitePixel[] = {255, 255, 255, 255};
        glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, whitePixel );

        glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
        glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
        glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT );
        glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT );

        return std::shared_ptr<TTexture>( new TTexture( textureID, TextureFormat::RGBA8, glm::uvec2( 1, 1 ) ) );
    }

    std::shared_ptr<TTexture> TTexture::createBlackTexture()
    {
        GLuint textureID;
        glGenTextures( 1, &textureID );
        glBindTexture( GL_TEXTURE_2D, textureID );

        unsigned char blackPixel[] = {0, 0, 0, 255};
        glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, blackPixel );

        glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
        glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
        glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT );
        glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT );

        return std::shared_ptr<TTexture>( new TTexture( textureID, TextureFormat::RGBA8, glm::uvec2( 1, 1 ) ) );
    }

    std::shared_ptr<TTexture> TTexture::createDefaultNormalMap()
    {
        GLuint textureID;
        glGenTextures( 1, &textureID );
        glBindTexture( GL_TEXTURE_2D, textureID );

        unsigned char normalPixel[] = {128, 128, 255, 255}; // Flat normal map
        glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, normalPixel );

        glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
        glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
        glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT );
        glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT );

        return std::shared_ptr<TTexture>( new TTexture( textureID, TextureFormat::RGBA8, glm::uvec2( 1, 1 ) ) );
    }

    std::shared_ptr<TTexture> TTexture::createFromFile( const std::string& p_path,
                                                      TextureFormat      p_format,
                                                      bool               p_generateMipmaps )
    {
        int width, height, channels;
        stbi_set_flip_vertically_on_load( true );
        unsigned char* data = stbi_load( p_path.c_str(), &width, &height, &channels, 0 );

        if ( !data )
        {
            TLOG_ERROR() << "Failed to load texture: " << p_path;
            return nullptr;
        }

        GLuint textureID;
        glGenTextures( 1, &textureID );
        glBindTexture( GL_TEXTURE_2D, textureID );

        GLenum internalFormat = toGLInternalFormat( p_format );
        GLenum dataFormat     = GL_RGBA;

        switch ( channels )
        {
            case 1:
                dataFormat = GL_RED;
                break;
            case 2:
                dataFormat = GL_RG;
                break;
            case 3:
                dataFormat = GL_RGB;
                break;
            case 4:
                dataFormat = GL_RGBA;
                break;
        }

        glTexImage2D( GL_TEXTURE_2D, 0, internalFormat, width, height, 0, dataFormat, GL_UNSIGNED_BYTE, data );

        if ( p_generateMipmaps )
        {
            glGenerateMipmap( GL_TEXTURE_2D );
        }

        // Set default parameters
        glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT );
        glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT );
        glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, p_generateMipmaps ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR );
        glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );

        stbi_image_free( data );

        return std::shared_ptr<TTexture>( new TTexture( textureID, p_format, glm::uvec2( width, height ) ) );
    }

    std::shared_ptr<TTexture> TTexture::createFromMemory( const unsigned char* p_data, size_t p_length, TextureFormat p_format )
    {
        int width, height, channels;
        stbi_set_flip_vertically_on_load( true );
        unsigned char* data = stbi_load_from_memory( p_data, p_length, &width, &height, &channels, 0 );

        if ( !data )
        {
            TLOG_ERROR() << "Failed to load texture from memory";
            return nullptr;
        }

        GLuint textureID;
        glGenTextures( 1, &textureID );
        glBindTexture( GL_TEXTURE_2D, textureID );

        GLenum internalFormat = toGLInternalFormat( p_format );
        GLenum dataFormat     = GL_RGBA;

        switch ( channels )
        {
            case 1:
                dataFormat = GL_RED;
                break;
            case 2:
                dataFormat = GL_RG;
                break;
            case 3:
                dataFormat = GL_RGB;
                break;
            case 4:
                dataFormat = GL_RGBA;
                break;
        }

        glTexImage2D( GL_TEXTURE_2D, 0, internalFormat, width, height, 0, dataFormat, GL_UNSIGNED_BYTE, data );

        // Set default parameters
        glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT );
        glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT );
        glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
        glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );

        stbi_image_free( data );

        return std::shared_ptr<TTexture>( new TTexture( textureID, p_format, glm::uvec2( width, height ) ) );
    }

    std::shared_ptr<TTexture> TTexture::createFromPixels( const unsigned char* p_pixels, int p_width, int p_height, TextureFormat p_format )
    {
        GLuint textureID;
        glGenTextures( 1, &textureID );
        glBindTexture( GL_TEXTURE_2D, textureID );

        GLenum internalFormat = toGLInternalFormat( p_format );
        GLenum dataFormat     = toGLFormat( p_format );

        glTexImage2D( GL_TEXTURE_2D, 0, internalFormat, p_width, p_height, 0, dataFormat, GL_UNSIGNED_BYTE, p_pixels );

        // Set default parameters
        glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT );
        glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT );
        glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
        glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );

        return std::shared_ptr<TTexture>( new TTexture( textureID, p_format, glm::uvec2( p_width, p_height ) ) );
    }


    std::shared_ptr<TTexture> TTexture::create( TextureFormat     p_format,
                                              const glm::uvec2& p_size,
                                              bool              p_generateMipmaps )
    {
        GLuint textureID;
        glGenTextures( 1, &textureID );
        glBindTexture( GL_TEXTURE_2D, textureID );

        GLenum internalFormat = toGLInternalFormat( p_format );
        GLenum formatEnum     = toGLFormat( p_format );
        GLenum dataType       = toGLDataType( p_format );

        glTexImage2D( GL_TEXTURE_2D, 0, internalFormat, p_size.x, p_size.y, 0, formatEnum, dataType, nullptr );

        if ( p_generateMipmaps )
        {
            glGenerateMipmap( GL_TEXTURE_2D );
        }

        // Set default parameters
        glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
        glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
        glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, p_generateMipmaps ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR );
        glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );

        return std::shared_ptr<TTexture>( new TTexture( textureID, p_format, p_size ) );
    }

    TTexture::~TTexture()
    {
        glDeleteTextures( 1, &m_id );
    }

    void TTexture::cleanup()
    {
        if ( m_id )
        {
            glDeleteTextures( 1, &m_id );
            m_id = 0;
        }
    }

    void TTexture::bind( GLuint p_unit ) const
    {
        glActiveTexture( GL_TEXTURE0 + p_unit );
        glBindTexture( GL_TEXTURE_2D, m_id );
    }

    void TTexture::unbind( GLuint p_unit )
    {
        glActiveTexture( GL_TEXTURE0 + p_unit );
        glBindTexture( GL_TEXTURE_2D, 0 );
    }

    void TTexture::resize( const glm::uvec2& p_newSize )
    {
        m_size = p_newSize;
        glBindTexture( GL_TEXTURE_2D, m_id );

        GLenum format         = toGLFormat( m_format );
        GLenum internalFormat = toGLInternalFormat( m_format );
        GLenum dataType       = toGLDataType( m_format );

        glTexImage2D( GL_TEXTURE_2D, 0, internalFormat, p_newSize.x, p_newSize.y, 0, format, dataType, nullptr );

        if ( m_hasMipmaps )
        {
            glGenerateMipmap( GL_TEXTURE_2D );
        }
    }

    void TTexture::generateMipmaps()
    {
        if ( !m_hasMipmaps )
        {
            glBindTexture( GL_TEXTURE_2D, m_id );
            glGenerateMipmap( GL_TEXTURE_2D );
            m_hasMipmaps = true;
        }
    }

    void TTexture::setFilter( TextureFilter p_minFilter, TextureFilter p_magFilter ) const
    {
        glBindTexture( GL_TEXTURE_2D, m_id );
        glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, toGLFilter( p_minFilter ) );
        glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, toGLFilter( p_magFilter ) );
    }

    void TTexture::setWrap( TextureWrap p_wrapS, TextureWrap p_wrapT ) const
    {
        glBindTexture( GL_TEXTURE_2D, m_id );
        glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, toGLWrap( p_wrapS ) );
        glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, toGLWrap( p_wrapT ) );
    }

    TTexture::TTexture( GLuint p_id, TextureFormat p_format, const glm::uvec2& p_size ) :
        m_id( p_id ), m_format( p_format ), m_size( p_size ), m_hasMipmaps( false )
    {
    }

    GLenum TTexture::toGLFormat( TextureFormat p_format )
    {
        switch ( p_format )
        {
            case TextureFormat::R8:
                return GL_RED;
            case TextureFormat::RG8:
                return GL_RG;
            case TextureFormat::RGB8:
            case TextureFormat::SRGB8:
                return GL_RGB;
            case TextureFormat::RGBA8:
            case TextureFormat::SRGBA8:
                return GL_RGBA;
            case TextureFormat::RGB16F:
                return GL_RGB;
            case TextureFormat::RGBA16F:
                return GL_RGBA;
            case TextureFormat::Depth16:
            case TextureFormat::Depth24:
            case TextureFormat::Depth32F:
                return GL_DEPTH_COMPONENT;
            default:
                return GL_RGBA;
        }
    }

    GLenum TTexture::toGLInternalFormat( TextureFormat p_format )
    {
        switch ( p_format )
        {
            case TextureFormat::R8:
                return GL_R8;
            case TextureFormat::RG8:
                return GL_RG8;
            case TextureFormat::RGB8:
                return GL_RGB8;
            case TextureFormat::RGBA8:
                return GL_RGBA8;
            case TextureFormat::SRGB8:
                return GL_SRGB8;
            case TextureFormat::SRGBA8:
                return GL_SRGB8_ALPHA8;
            case TextureFormat::RGB16F:
                return GL_RGB16F;
            case TextureFormat::RGBA16F:
                return GL_RGBA16F;
            case TextureFormat::Depth16:
                return GL_DEPTH_COMPONENT16;
            case TextureFormat::Depth24:
                return GL_DEPTH_COMPONENT24;
            case TextureFormat::Depth32F:
                return GL_DEPTH_COMPONENT32F;
            default:
                return GL_RGBA8;
        }
    }

    GLenum TTexture::toGLDataType( TextureFormat p_format )
    {
        switch ( p_format )
        {
            case TextureFormat::RGB16F:
            case TextureFormat::RGBA16F:
                return GL_FLOAT;
            case TextureFormat::Depth32F:
                return GL_FLOAT;
            default:
                return GL_UNSIGNED_BYTE;
        }
    }

    GLenum TTexture::toGLFilter( TextureFilter p_filter )
    {
        switch ( p_filter )
        {
            case TextureFilter::Nearest:
                return GL_NEAREST;
            case TextureFilter::Linear:
                return GL_LINEAR;
            case TextureFilter::NearestMipmapNearest:
                return GL_NEAREST_MIPMAP_NEAREST;
            case TextureFilter::LinearMipmapNearest:
                return GL_LINEAR_MIPMAP_NEAREST;
            case TextureFilter::NearestMipmapLinear:
                return GL_NEAREST_MIPMAP_LINEAR;
            case TextureFilter::LinearMipmapLinear:
                return GL_LINEAR_MIPMAP_LINEAR;
            default:
                return GL_LINEAR;
        }
    }

    GLenum TTexture::toGLWrap( TextureWrap p_wrap )
    {
        switch ( p_wrap )
        {
            case TextureWrap::Repeat:
                return GL_REPEAT;
            case TextureWrap::MirroredRepeat:
                return GL_MIRRORED_REPEAT;
            case TextureWrap::ClampToEdge:
                return GL_CLAMP_TO_EDGE;
            case TextureWrap::ClampToBorder:
                return GL_CLAMP_TO_BORDER;
            default:
                return GL_REPEAT;
        }
    }
} // namespace Tomos
