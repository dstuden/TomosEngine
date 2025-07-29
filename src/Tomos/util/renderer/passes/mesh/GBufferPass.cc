//
// Created by dstuden on 7/14/25.
//

#include "GBufferPass.hh"

#include "Tomos/core/Application.hh"
#include "Tomos/systems/camera/CameraSystem.hh"
#include "Tomos/systems/mesh/MeshSystem.hh"
#include "Tomos/util/renderer/Renderer.hh"

namespace Tomos
{
    GBufferPass::GBufferPass( unsigned int p_width, unsigned int p_height, const std::string& p_layerId ) : RenderPass( "GBufferPass", p_layerId )
    {
        std::vector<TextureFormat> colorFormats = {
                TextureFormat::SRGBA8,  // baseColor
                TextureFormat::RGBA16F,  // normal
                TextureFormat::RG8,  // metallic + roughness
                TextureFormat::RGBA8  // emission
        };

        m_frameBuffer = std::make_shared<FrameBuffer>( p_width, p_height, colorFormats, TextureFormat::Depth24 );
    }

    void GBufferPass::resize( unsigned int p_width, unsigned int p_height ) { m_frameBuffer->resize( p_width, p_height ); }

    void GBufferPass::execute()
    {
        m_frameBuffer->bind();
        Renderer::setClearedColor( { 0.0f, 0.0f, 0.0, 1 } );
        Renderer::clear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );

        auto        meshSystem = Application::getState().ecs().getSystem<MeshSystem>();
        const auto& drawCalls  = meshSystem.getDrawCalls( m_layerId );

        Renderer::beginBatch();

        for ( const auto& drawCall : drawCalls )
        {
            Renderer::addToBatch( drawCall.m_shader, drawCall.m_material, drawCall.m_vertexArray, drawCall.m_transform );
        }

        Renderer::endBatch( Application::getState().ecs().getSystem<CameraSystem>().getViewProjectionMat( m_layerId ) );

        Renderer::clearFrameBuffer();
    }
}  // namespace Tomos
