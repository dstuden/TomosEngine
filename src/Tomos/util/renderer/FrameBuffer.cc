#include "FrameBuffer.hh"

#include "Tomos/util/logger/Logger.hh"

namespace Tomos
{
    FrameBuffer::FrameBuffer( unsigned int p_width, unsigned int p_height, const std::vector<TextureFormat>& p_colorFormats, TextureFormat p_depthFormat ) :
        m_size( p_width, p_height ), m_colorFormats( p_colorFormats ), m_depthFormat( p_depthFormat )
    {
        initialize();
    }

    FrameBuffer::~FrameBuffer() { cleanup(); }

    void FrameBuffer::initialize()
    {
        glGenFramebuffers( 1, &m_fbo );
        glBindFramebuffer( GL_FRAMEBUFFER, m_fbo );

        m_colorTextures.clear();
        std::vector<GLenum> drawBuffers;

        for ( unsigned int i = 0; i < m_colorFormats.size(); ++i )
        {
            auto tex = Texture::create( m_colorFormats[i], m_size );
            tex->setFilter( TextureFilter::Linear, TextureFilter::Linear );
            tex->setWrap( TextureWrap::ClampToEdge, TextureWrap::ClampToEdge );

            glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, GL_TEXTURE_2D, tex->getID(), 0 );

            m_colorTextures.push_back( tex );
            drawBuffers.push_back( GL_COLOR_ATTACHMENT0 + i );
        }

        if ( !drawBuffers.empty() )
        {
            glDrawBuffers( static_cast<GLsizei>( drawBuffers.size() ), drawBuffers.data() );
        }
        else
        {
            glDrawBuffer( GL_NONE );
        }

        m_depthTexture = Texture::create( m_depthFormat, m_size );
        m_depthTexture->setFilter( TextureFilter::Nearest, TextureFilter::Nearest );
        m_depthTexture->setWrap( TextureWrap::ClampToEdge, TextureWrap::ClampToEdge );

        glFramebufferTexture2D( GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, m_depthTexture->getID(), 0 );

        if ( glCheckFramebufferStatus( GL_FRAMEBUFFER ) != GL_FRAMEBUFFER_COMPLETE )
        {
            LOG_ERROR() << "Framebuffer is not complete!";
            cleanup();
        }

        glBindFramebuffer( GL_FRAMEBUFFER, 0 );
    }

    void FrameBuffer::cleanup()
    {
        if ( m_fbo )
        {
            glDeleteFramebuffers( 1, &m_fbo );
            m_fbo = 0;
        }
        m_colorTextures.clear();
        m_depthTexture.reset();
    }

    void FrameBuffer::bind() const
    {
        if ( !isValid() ) return;
        glBindFramebuffer( GL_FRAMEBUFFER, m_fbo );
        glViewport( 0, 0, m_size.x, m_size.y );
    }

    void FrameBuffer::unbind() const { glBindFramebuffer( GL_FRAMEBUFFER, 0 ); }

    void FrameBuffer::resize( unsigned int p_width, unsigned int p_height )
    {
        if ( p_width == m_size.x && p_height == m_size.y ) return;
        m_size = { p_width, p_height };
        cleanup();
        initialize();
    }
}  // namespace Tomos
