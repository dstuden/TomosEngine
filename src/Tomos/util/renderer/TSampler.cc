//
// Created by dstuden on 4/8/25.
//

#include "TSampler.hh"
#include "Tomos/util/logger/TLogger.hh"

namespace Tomos
{
    std::shared_ptr<TSampler> TSampler::create()
    {
        return create( SamplerFilter::Linear,
                       SamplerFilter::Linear,
                       SamplerWrap::Repeat,
                       SamplerWrap::Repeat );
    }

    std::shared_ptr<TSampler> TSampler::create( SamplerFilter p_minFilter,
                                              SamplerFilter p_magFilter,
                                              SamplerWrap   p_wrapS,
                                              SamplerWrap   p_wrapT,
                                              float         p_maxAnisotropy )
    {
        GLuint samplerID;
        glGenSamplers( 1, &samplerID );

        // Set filtering parameters
        glSamplerParameteri( samplerID, GL_TEXTURE_MIN_FILTER, toGLFilter( p_minFilter ) );
        glSamplerParameteri( samplerID, GL_TEXTURE_MAG_FILTER, toGLFilter( p_magFilter ) );

        // Set wrapping parameters
        glSamplerParameteri( samplerID, GL_TEXTURE_WRAP_S, toGLWrap( p_wrapS ) );
        glSamplerParameteri( samplerID, GL_TEXTURE_WRAP_T, toGLWrap( p_wrapT ) );

        // Set anisotropy if supported and requested
        if ( p_maxAnisotropy > 1.0f )
        {
            if ( GLEW_EXT_texture_filter_anisotropic )
            {
                float maxSupported;
                glGetFloatv( GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &maxSupported );
                float actualAniso = ( p_maxAnisotropy > maxSupported ) ? maxSupported : p_maxAnisotropy;
                glSamplerParameterf( samplerID, GL_TEXTURE_MAX_ANISOTROPY_EXT, actualAniso );
            }
            else
            {
                TLOG_WARN() << "Anisotropic filtering requested but not supported";
            }
        }

        return std::shared_ptr<TSampler>( new TSampler( samplerID ) );
    }

    TSampler::~TSampler()
    {
        glDeleteSamplers( 1, &m_id );
    }

    void TSampler::bind( GLuint p_unit ) const
    {
        glBindSampler( p_unit, m_id );
    }

    void TSampler::unbind( GLuint p_unit ) const
    {
        glBindSampler( p_unit, 0 );
    }

    void TSampler::setFilter( SamplerFilter p_minFilter, SamplerFilter p_magFilter )
    {
        glSamplerParameteri( m_id, GL_TEXTURE_MIN_FILTER, toGLFilter( p_minFilter ) );
        glSamplerParameteri( m_id, GL_TEXTURE_MAG_FILTER, toGLFilter( p_magFilter ) );
    }

    void TSampler::setWrap( SamplerWrap p_wrapS, SamplerWrap p_wrapT )
    {
        glSamplerParameteri( m_id, GL_TEXTURE_WRAP_S, toGLWrap( p_wrapS ) );
        glSamplerParameteri( m_id, GL_TEXTURE_WRAP_T, toGLWrap( p_wrapT ) );
    }

    void TSampler::setAnisotropy( float p_anisotropy )
    {
        if ( GLEW_EXT_texture_filter_anisotropic && p_anisotropy > 1.0f )
        {
            float maxSupported;
            glGetFloatv( GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &maxSupported );
            float actualAniso = ( p_anisotropy > maxSupported ) ? maxSupported : p_anisotropy;
            glSamplerParameterf( m_id, GL_TEXTURE_MAX_ANISOTROPY_EXT, actualAniso );
        }
    }

    TSampler::TSampler( GLuint p_id ) :
        m_id( p_id )
    {
    }

    GLenum TSampler::toGLFilter( SamplerFilter p_filter )
    {
        switch ( p_filter )
        {
            case SamplerFilter::Nearest:
                return GL_NEAREST;
            case SamplerFilter::Linear:
                return GL_LINEAR;
            case SamplerFilter::NearestMipmapNearest:
                return GL_NEAREST_MIPMAP_NEAREST;
            case SamplerFilter::LinearMipmapNearest:
                return GL_LINEAR_MIPMAP_NEAREST;
            case SamplerFilter::NearestMipmapLinear:
                return GL_NEAREST_MIPMAP_LINEAR;
            case SamplerFilter::LinearMipmapLinear:
                return GL_LINEAR_MIPMAP_LINEAR;
            default:
                return GL_LINEAR;
        }
    }

    GLenum TSampler::toGLWrap( SamplerWrap p_wrap )
    {
        switch ( p_wrap )
        {
            case SamplerWrap::Repeat:
                return GL_REPEAT;
            case SamplerWrap::MirroredRepeat:
                return GL_MIRRORED_REPEAT;
            case SamplerWrap::ClampToEdge:
                return GL_CLAMP_TO_EDGE;
            case SamplerWrap::ClampToBorder:
                return GL_CLAMP_TO_BORDER;
            default:
                return GL_REPEAT;
        }
    }

    std::shared_ptr<TSampler> TSampler::createLinearRepeat()
    {
        return create( SamplerFilter::Linear,
                       SamplerFilter::Linear,
                       SamplerWrap::Repeat,
                       SamplerWrap::Repeat );
    }

    std::shared_ptr<TSampler> TSampler::createNearestRepeat()
    {
        return create( SamplerFilter::Nearest,
                       SamplerFilter::Nearest,
                       SamplerWrap::Repeat,
                       SamplerWrap::Repeat );
    }

    std::shared_ptr<TSampler> TSampler::createLinearClamp()
    {
        return create( SamplerFilter::Linear,
                       SamplerFilter::Linear,
                       SamplerWrap::ClampToEdge,
                       SamplerWrap::ClampToEdge );
    }

    std::shared_ptr<TSampler> TSampler::createNearestClamp()
    {
        return create( SamplerFilter::Nearest,
                       SamplerFilter::Nearest,
                       SamplerWrap::ClampToEdge,
                       SamplerWrap::ClampToEdge );
    }

    std::shared_ptr<TSampler> TSampler::createAnisotropicRepeat( float p_anisotropy )
    {
        return create( SamplerFilter::LinearMipmapLinear,
                       SamplerFilter::Linear,
                       SamplerWrap::Repeat,
                       SamplerWrap::Repeat,
                       p_anisotropy );
    }
} // namespace Tomos
