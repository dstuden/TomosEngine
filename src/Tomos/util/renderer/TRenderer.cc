#include "TRenderer.hh"

#include "Tomos/core/TApplication.hh"
#include "Tomos/util/conf/TConfig.hh"
#include "Tomos/util/logger/TLogger.hh"

namespace Tomos
{
    static std::shared_ptr<TShader>      g_defaultPostProcessShader;

    std::unordered_map<MaterialBatchKey, std::unordered_map<std::shared_ptr<TVertexArray>, BatchData>, MaterialBatchKeyHash> TRenderer::g_batches;
    bool                                                                                                                    TRenderer::g_batching = false;

    std::unordered_map<std::shared_ptr<TVertexArray>, std::shared_ptr<TStorageBuffer>> TRenderer::g_instanceBuffers;

    void TRenderer::setClearedColor( const glm::vec4& p_color ) { glClearColor( p_color.r, p_color.g, p_color.b, p_color.a ); }

    void TRenderer::clear( unsigned int p_mask ) { glClear( p_mask ); }

    void TRenderer::onWindowResize( unsigned int p_width, unsigned int p_height )
    {
        // glViewport(0, 0, p_width, p_height);
    }

    void TRenderer::beginBatch()
    {
        g_batching = true;
        g_batches.clear();
    }

    void TRenderer::endBatch( const glm::mat4& p_viewProjection )
    {
        if ( !g_batching ) return;

        for ( auto& [materialKey, meshBatches] : g_batches )
        {
            materialKey.m_shader->bind();
            materialKey.m_material->bind();
            materialKey.m_shader->setMat4( "uViewProjection", p_viewProjection );

            for ( auto& [vertexArray, batchData] : meshBatches )
            {
                // Convert batch to instanced rendering
                std::vector<InstanceData> instances;
                instances.reserve( batchData.m_transforms.size() );
                for ( const auto& transform : batchData.m_transforms )
                {
                    instances.emplace_back( InstanceData{ transform } );
                }

                drawInstanced( materialKey.m_shader, materialKey.m_material, vertexArray, instances, p_viewProjection, true );
            }
        }

        g_batching = false;
        g_batches.clear();
    }

    void TRenderer::addToBatch( const std::shared_ptr<TShader>& p_shader, const std::shared_ptr<TMaterial>& p_material,
                               const std::shared_ptr<TVertexArray>& p_vertexArray, const glm::mat4& p_transform )
    {
        if ( !g_batching ) return;

        MaterialBatchKey key{ p_shader, p_material };
        auto&            batchData = g_batches[key][p_vertexArray];
        if ( !batchData.m_vertexArray )
        {
            batchData.m_vertexArray = p_vertexArray;
        }
        batchData.m_transforms.push_back( p_transform );
    }

    void TRenderer::draw( const std::shared_ptr<TShader>& p_shader, const std::shared_ptr<TMaterial>& p_material,
                         const std::shared_ptr<TVertexArray>& p_vertexArray, const glm::mat4& p_transform, const glm::mat4& p_viewProjection )
    {
        if ( g_batching )
        {
            addToBatch( p_shader, p_material, p_vertexArray, p_transform );
            return;
        }

        // Fallback to immediate mode if not batching
        if ( !p_vertexArray || !p_vertexArray->getIndexBuffer() )
        {
            TLOG_ERROR() << "Renderer::draw() - Invalid VertexArray or IndexBuffer";
            return;
        }

        p_shader->bind();
        p_material->bind();
        p_shader->setMat4( "uTransform", p_transform );
        p_shader->setMat4( "uViewProjection", p_viewProjection );

        p_vertexArray->bind();
        glDrawElements( GL_TRIANGLES, p_vertexArray->getIndexBuffer()->getCount(), GL_UNSIGNED_INT, nullptr );

        // Error checking
        GLenum err;
        while ( ( err = glGetError() ) != GL_NO_ERROR )
        {
            TLOG_ERROR() << "OpenGL error in Renderer::draw(): " << err;
        }
    }

    void TRenderer::drawInstanced( const std::shared_ptr<TShader>& p_shader, const std::shared_ptr<TMaterial>& p_material,
                                  const std::shared_ptr<TVertexArray>& p_vertexArray, const std::vector<InstanceData>& p_instances,
                                  const glm::mat4& p_viewProjection, bool p_batched )
    {
        if ( !p_vertexArray || !p_vertexArray->getIndexBuffer() )
        {
            TLOG_ERROR() << "Invalid VertexArray or IndexBuffer";
            return;
        }

        if ( !p_batched )
        {
            p_shader->bind();
            p_material->bind();
        }

        p_shader->setMat4( "uViewProjection", p_viewProjection );

        p_vertexArray->bind();

        size_t instanceCount  = p_instances.size();
        size_t instancesDrawn = 0;

        auto maxInstances = Global::config.get<size_t>( "maxInstancesPerDraw" );

        while ( instancesDrawn < instanceCount )
        {
            size_t batchSize = std::min( maxInstances, instanceCount - instancesDrawn );

            auto& instanceBuffer = g_instanceBuffers[p_vertexArray];
            if ( !instanceBuffer || instanceBuffer->getSize() < sizeof( InstanceData ) * maxInstances )
            {
                instanceBuffer = std::make_shared<TStorageBuffer>( sizeof( InstanceData ) * maxInstances );
            }

            instanceBuffer->setData( p_instances.data() + instancesDrawn, sizeof( InstanceData ) * batchSize );

            instanceBuffer->bindBase( 0 );  // Binding point 0

            glDrawElementsInstanced( GL_TRIANGLES, p_vertexArray->getIndexBuffer()->getCount(), GL_UNSIGNED_INT, nullptr, static_cast<GLsizei>( batchSize ) );

            instancesDrawn += batchSize;
        }

        // Error checking
        GLenum err;
        while ( ( err = glGetError() ) != GL_NO_ERROR )
        {
            TLOG_ERROR() << "OpenGL error: " << err;
        }
    }

    void TRenderer::clearFrameBuffer() { glBindFramebuffer( GL_FRAMEBUFFER, 0 ); }
    
    void TRenderer::renderLayerFrameBufferToQuad( const std::shared_ptr<LayerFrameBuffer>& p_framebuffer, const std::shared_ptr<TVertexArray>& p_quad,
                                            const std::shared_ptr<TShader>& p_postProcessShader )
    {
        if ( !p_framebuffer || !p_quad ) return;

        glDisable( GL_DEPTH_TEST );
        glEnable( GL_BLEND );
        glBlendFunc( GL_ONE, GL_ONE_MINUS_SRC_ALPHA );

        // Use default shader if none provided
        auto shaderToUse = p_postProcessShader ? p_postProcessShader : g_defaultPostProcessShader;
        if ( !shaderToUse )
        {
            // Create default post-process shader if not exists
            g_defaultPostProcessShader = TResourceManager::getShader( "default_post_process" );
            if ( !g_defaultPostProcessShader )
            {
                g_defaultPostProcessShader = std::make_shared<TShader>( TResourceManager::getShaderPath( "screen_quad_vertex.glsl" ),
                                                                       TResourceManager::getShaderPath( "screen_quad_fragment.glsl" ) );
                TResourceManager::cacheShader( "default_post_process", g_defaultPostProcessShader );
            }
            shaderToUse = g_defaultPostProcessShader;
        }

        shaderToUse->bind();
        p_framebuffer->getColorTexture()->bind( 0 );
        shaderToUse->setInt( "uScreenTexture", 0 );

        p_quad->bind();
        glDrawArrays( GL_TRIANGLES, 0, 6 );
        glDisable( GL_BLEND );
        glEnable( GL_DEPTH_TEST );
    }
}  // namespace Tomos
