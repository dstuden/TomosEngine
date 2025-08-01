//
// Created by dstuden on 7/29/25.
//

#include "TLightPass.hh"

#include "Tomos/core/TApplication.hh"
#include "Tomos/systems/camera/TCameraComponent.hh"
#include "Tomos/systems/camera/TCameraSystem.hh"
#include "Tomos/systems/light/TLightSystem.hh"
#include "Tomos/util/renderer/TRenderer.hh"

namespace Tomos
{
    TLightPass::TLightPass( const std::shared_ptr<TGBufferPass>& p_gBufferPass, const std::string& p_name, const std::shared_ptr<TLayer>& p_layer ) :
        TRenderPass( p_name, p_layer->getLayerId() )
    {
        m_layer       = p_layer;
        m_gBufferPass = p_gBufferPass;
    }

    void TLightPass::execute()
    {
        m_layer->getLayerFramebuffer()->bind();
        TRenderer::setClearedColor( { 0, 0, 0, 0 } );
        TRenderer::clear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );

        auto shader = TResourceManager::getShader( "light_pass" );
        if ( !shader )
        {
            shader = std::make_shared<TShader>( TResourceManager::getShaderPath( "screen_quad_vertex.glsl" ),
                                                TResourceManager::getShaderPath( "light_fragment.glsl" ) );
            TResourceManager::cacheShader( "light_pass", shader );
        }
        shader->bind();

        m_gBufferPass->getFrameBuffer()->getColorTexture( 0 )->bind( 0 );  // uGBase
        shader->setInt( "uGBase", 0 );
        m_gBufferPass->getFrameBuffer()->getColorTexture( 1 )->bind( 1 );  // uGNormal
        shader->setInt( "uGNormal", 1 );
        m_gBufferPass->getFrameBuffer()->getColorTexture( 2 )->bind( 2 );  // uGMtlRgh
        shader->setInt( "uGMtlRgh", 2 );
        m_gBufferPass->getFrameBuffer()->getColorTexture( 3 )->bind( 3 );  // uGEmission
        shader->setInt( "uGEmission", 3 );
        m_gBufferPass->getFrameBuffer()->getDepthTexture()->bind( 4 );  // uDepth
        shader->setInt( "uDepth", 4 );

        auto& lightSystem = TApplication::getState().ecs().getSystem<TLightSystem>();
        if ( lightSystem.getLightsBuffer( m_layerId ) ) lightSystem.getLightsBuffer( m_layerId )->bindBase( 0 );  // binding = 0 in shader

        // 5. Camera info
        auto&     cameraSystem = TApplication::getState().ecs().getSystem<TCameraSystem>();
        auto      cameraNode   = cameraSystem.getActiveCameraNode( m_layerId );
        glm::vec3 cameraPos    = cameraNode ? glm::vec3( cameraNode->getTransform().m_globMat[3] ) : glm::vec3( 0.0f );

        glm::mat4 inverseViewProj = cameraSystem.getViewProjectionInvMat( m_layerId );
        shader->setVec3( "uCameraPos", cameraPos );
        shader->setMat4( "uInverseViewProj", inverseViewProj );

        std::shared_ptr<TVertexArray> quad = m_layer->getQuad();
        if ( quad )
        {
            quad->bind();
            glDrawArrays( GL_TRIANGLES, 0, 6 );
        }
        m_layer->getLayerFramebuffer()->unbind();
    }

    void TLightPass::resize( unsigned int p_width, unsigned int p_height ) {}


}  // namespace Tomos
