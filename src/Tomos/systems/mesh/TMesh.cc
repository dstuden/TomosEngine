#include "../../systems/mesh/TMesh.hh"

#include "Tomos/util/logger/TLogger.hh"
#include "Tomos/util/renderer/TRenderer.hh"

namespace Tomos
{
    TMesh::TMesh( const std::shared_ptr<TVertexBuffer>& p_position, const std::shared_ptr<TVertexBuffer>& p_normal, const std::shared_ptr<TVertexBuffer>& p_texCoord,
                const std::shared_ptr<TVertexBuffer>& p_tangent, const std::shared_ptr<TIndexBuffer>& p_index, const std::shared_ptr<TShader>& p_shader ) :
        m_position( p_position ), m_normal( p_normal ), m_texCoord( p_texCoord ), m_tangent( p_tangent ), m_index( p_index ), m_shader( p_shader )
    {
        m_vertexArray = std::make_shared<TVertexArray>();

        setupVertexAttributes();

        // Bind index buffer if present
        if ( m_index )
        {
            m_vertexArray->setIndexBuffer( m_index );
        }
    }

    void TMesh::setupVertexAttributes() const
    {
        // YES we use asserts here
        // deal with it
        // https://tenor.com/pU4HDPnKDf7.gif

        TLOG_ASSERT_MSG( m_position, "Mesh requires a position buffer" );

        // Position (location = 0)
        TBufferLayout positionLayout = { { ShaderDataType::Float3, "aPosition" } };
        m_position->setLayout( positionLayout );
        m_vertexArray->addVertexBuffer( m_position );

        // Normal (location = 1)
        TLOG_ASSERT_MSG( m_normal, "Mesh requires a normal buffer" );
        TBufferLayout normalLayout = { { ShaderDataType::Float3, "aNormal" } };
        m_normal->setLayout( normalLayout );
        m_vertexArray->addVertexBuffer( m_normal );


        // Texture Coordinates (location = 2)
        TLOG_ASSERT_MSG( m_texCoord, "Mesh requires a texture coordinate buffer" );
        TBufferLayout texCoordLayout = { { ShaderDataType::Float2, "aTexCoord" } };
        m_texCoord->setLayout( texCoordLayout );
        m_vertexArray->addVertexBuffer( m_texCoord );


        // Tangent (location = 3)
        TLOG_ASSERT_MSG( m_normal, "Mesh requires a normal buffer to set up tangents" );
        TBufferLayout tangentLayout = { { ShaderDataType::Float4, "aTangent" } };
        m_tangent->setLayout( tangentLayout );
        m_vertexArray->addVertexBuffer( m_tangent );
    }
}  // namespace Tomos
