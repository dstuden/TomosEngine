//
// Created by dstuden on 7/15/25.
//

#include "LightPass.hh"

#include "Tomos/core/Application.hh"
#include "Tomos/systems/camera/CameraSystem.hh"
#include "Tomos/systems/light/LightSystem.hh"
#include "Tomos/util/renderer/Renderer.hh"
#include "Tomos/util/renderer/Shader.hh"
#include "Tomos/util/renderer/VertexArray.hh"
#include "Tomos/util/resourceManager/ResourceManager.hh"

namespace Tomos
{
    LightPass::LightPass( const std::shared_ptr<FrameBuffer>& p_gbuffer, const std::shared_ptr<Layer>& p_layer ) :
        RenderPass( "LightPass", p_layer->getLayerId() )
    {
        m_layer   = p_layer;
        m_gbuffer = p_gbuffer;
    }

    void LightPass::resize( unsigned int p_width, unsigned int p_height )
    {
        if ( m_layer->getLayerFramebuffer() ) m_layer->getLayerFramebuffer()->resize( p_width, p_height );
    }

    void LightPass::execute()
    {
        // 1. Bind output framebuffer
        m_layer->getLayerFramebuffer()->bind();
        Renderer::setClearedColor( { 0, 0, 0, 1 } );
        Renderer::clear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );

        // 2. Get or load shader
        auto shader = ResourceManager::getShader( "light_pass" );
        if ( !shader )
        {
            shader = std::make_shared<Shader>( ResourceManager::getShaderPath( "screen_quad_vertex.glsl" ),
                                               ResourceManager::getShaderPath( "light_fragment.glsl" ) );
            ResourceManager::cacheShader( "light_pass", shader );
        }
        shader->bind();

        // 3. Bind G-buffer textures
        m_gbuffer->getColorTexture( 0 )->bind( 0 );  // uGBase
        shader->setInt( "uGBase", 0 );
        m_gbuffer->getColorTexture( 1 )->bind( 1 );  // uGNormal
        shader->setInt( "uGNormal", 1 );
        m_gbuffer->getColorTexture( 2 )->bind( 2 );  // uGMtlRgh
        shader->setInt( "uGMtlRgh", 2 );
        m_gbuffer->getColorTexture( 3 )->bind( 3 );  // uGEmission
        shader->setInt( "uGEmission", 3 );
        m_gbuffer->getDepthTexture()->bind( 4 );  // uDepth
        shader->setInt( "uDepth", 4 );

        // 4. Bind SSBO: Lights
        auto& lightSystem = Application::getState().ecs().getSystem<LightSystem>();
        if ( lightSystem.getLightsBuffer( m_layerId ) ) lightSystem.getLightsBuffer( m_layerId )->bindBase( 0 );  // binding = 0 in shader

        // 5. Camera info
        auto&     cameraSystem = Application::getState().ecs().getSystem<CameraSystem>();
        auto      cameraNode   = cameraSystem.getActiveCameraNode( m_layerId );
        glm::vec3 cameraPos    = cameraNode ? glm::vec3( cameraNode->m_transform.m_globMat[3] ) : glm::vec3( 0.0f );

        glm::mat4 inverseViewProj = cameraSystem.getViewProjectionInvMat( m_layerId );
        shader->setVec3( "uCameraPos", cameraPos );
        shader->setMat4( "uInverseViewProj", inverseViewProj );

        std::shared_ptr<VertexArray> quad = m_layer->getQuad();
        if ( quad )
        {
            quad->bind();
            glDrawArrays( GL_TRIANGLES, 0, 6 );
        }
        m_layer->getLayerFramebuffer()->unbind();
    }


}  // namespace Tomos
