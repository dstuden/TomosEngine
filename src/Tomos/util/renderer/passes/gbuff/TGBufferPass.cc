//
// Created by dstuden on 7/14/25.
//

#include "TGBufferPass.hh"

#include "Tomos/core/TApplication.hh"
#include "Tomos/systems/camera/TCameraSystem.hh"
#include "Tomos/systems/mesh/TMeshSystem.hh"
#include "Tomos/util/renderer/TRenderer.hh"

namespace Tomos
{
    TGBufferPass::TGBufferPass( unsigned int p_width, unsigned int p_height, const std::string& p_layerId ) : TRenderPass( "GBufferPass", p_layerId )
    {
        std::vector<TextureFormat> colorFormats = {
                TextureFormat::SRGBA8,  // baseColor
                TextureFormat::RGBA16F,  // normal
                TextureFormat::RG8,  // metallic + roughness
                TextureFormat::RGBA8  // emission
        };

        m_frameBuffer = std::make_shared<TFrameBuffer>( p_width, p_height, colorFormats, TextureFormat::Depth24 );
    }

    void TGBufferPass::resize( unsigned int p_width, unsigned int p_height ) { m_frameBuffer->resize( p_width, p_height ); }

    void TGBufferPass::execute()
    {
        m_frameBuffer->bind();
        TRenderer::setClearedColor( { 0.0f, 0.0f, 0.0, 0 } );
        TRenderer::clear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );

        auto        meshSystem = TApplication::getState().ecs().getSystem<TMeshSystem>();
        const auto& drawCalls  = meshSystem.getDrawCalls( m_layerId );

        TRenderer::beginBatch();

        for ( const auto& drawCall : drawCalls )
        {
            TRenderer::addToBatch( drawCall.m_shader, drawCall.m_material, drawCall.m_vertexArray, drawCall.m_transform );
        }

        TRenderer::endBatch( TApplication::getState().ecs().getSystem<TCameraSystem>().getViewProjectionMat( m_layerId ) );

        TRenderer::clearFrameBuffer();
    }
}  // namespace Tomos
