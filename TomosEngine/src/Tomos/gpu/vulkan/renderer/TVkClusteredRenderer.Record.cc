#include "Tomos/gpu/vulkan/renderer/TVkClusteredRenderer.hh"

#include <algorithm>
#include <glm/glm.hpp>
#include <stdexcept>
#include <vector>

#include "Tomos/gpu/vulkan/TVkGpu.hh"
#include "Tomos/gpu/vulkan/TVkMaterial.hh"
#include "Tomos/gpu/vulkan/TVkMesh.hh"
#include "Tomos/gpu/vulkan/TVkUtil.hh"
#include "Tomos/util/math/TFrustum.hh"
#include "Tomos/util/math/TPointShadow.hh"
#include "Tomos/util/memory/TArenaAllocator.hh"
#include "Tomos/util/memory/TFrameAllocator.hh"
#include "Tomos/util/profile/TProfile.hh"

namespace Tomos
{
    void TVkClusteredRenderer::render( VkCommandBuffer p_cmd, uint32_t p_frameIndex, const TFrameState& p_state )
    {
        TOMOS_PROFILE_SCOPE( "GPU.Record" );

        TFrameResources& frame = m_frames[ p_frameIndex ];

#if TOMOS_DEBUG
        m_tsFrameIndex = p_frameIndex;
        if ( TFrameProfiler::get().isCaptureEnabled() )
        {
            m_gpuTimestamps.ensureCreated( m_gpu.device(), g_kFramesInFlight );
            m_gpuTimestamps.resolvePrevious( m_gpu.device(), p_frameIndex, m_gpu.timestampPeriod() );
            m_gpuTimestamps.beginRecord( p_cmd, p_frameIndex );
        }
#endif

        const uint32_t zero = 0;
        frame.m_counter.upload( &zero, 0, sizeof( zero ) );

        TOMOS_PROFILE_GPU_BEGIN( m_gpuTimestamps, p_cmd, p_frameIndex, Shadows );
        recordShadowPasses( p_cmd, p_state, frame );
        TOMOS_PROFILE_GPU_END( m_gpuTimestamps, p_cmd, p_frameIndex, Shadows );

        TOMOS_PROFILE_GPU_BEGIN( m_gpuTimestamps, p_cmd, p_frameIndex, ClusterCull );
        recordClusterCull( p_cmd, frame );
        TOMOS_PROFILE_GPU_END( m_gpuTimestamps, p_cmd, p_frameIndex, ClusterCull );

        recordForwardPass( p_cmd, p_state, frame );

        TOMOS_PROFILE_GPU_BEGIN( m_gpuTimestamps, p_cmd, p_frameIndex, ParticleSim );
        recordParticleSim( p_cmd, frame, static_cast<uint32_t>( p_state.m_emitters.size() ) );
        TOMOS_PROFILE_GPU_END( m_gpuTimestamps, p_cmd, p_frameIndex, ParticleSim );

        TOMOS_PROFILE_GPU_BEGIN( m_gpuTimestamps, p_cmd, p_frameIndex, ParticleDraw );
        recordParticlePass( p_cmd, p_state, frame );
        TOMOS_PROFILE_GPU_END( m_gpuTimestamps, p_cmd, p_frameIndex, ParticleDraw );

        recordPost( p_cmd, p_frameIndex, p_state );
    }

    void TVkClusteredRenderer::uploadParticles( uint32_t p_frameIndex, const TFrameState& p_state )
    {
        TFrameResources& frame = m_frames[ p_frameIndex ];

        TParticleSimUBO sim{};
        sim.m_dt           = p_state.m_particleDt;
        sim.m_emitterCount = static_cast<uint32_t>( p_state.m_emitters.size() );
        sim.m_maxParticles = g_kMaxParticles;
        sim.m_frameIndex   = m_particleFrameCounter++;
        frame.m_particleSimUBO.upload( &sim, 0, sizeof( sim ) );

        if ( !p_state.m_emitters.empty() )
        {
            frame.m_emitterBuf.upload( p_state.m_emitters.data(), 0, p_state.m_emitters.size() * sizeof( TEmitterData ) );
        }
    }

    void TVkClusteredRenderer::recordShadowPasses( VkCommandBuffer p_cmd, const TFrameState& p_state, const TFrameResources& p_frame )
    {
        VkUtil::imageBarrier( p_cmd, m_shadowMaps.handle(), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
                              VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT, VK_ACCESS_2_NONE,
                              VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
                              VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_IMAGE_ASPECT_DEPTH_BIT, g_kMaxShadowMaps );

        VkViewport viewport{};
        viewport.width    = static_cast<float>( g_kShadowMapSize );
        viewport.height   = static_cast<float>( g_kShadowMapSize );
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;

        VkRect2D scissor{};
        scissor.extent = { g_kShadowMapSize, g_kShadowMapSize };

        auto drawCasters = [ & ]( const glm::mat4& p_lightVP )
        {
            vkCmdBindDescriptorSets( p_cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_shadowPipeLayout, 0, 1, &p_frame.m_sceneSet, 0, nullptr );
            vkCmdPushConstants( p_cmd, m_shadowPipeLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof( glm::mat4 ), &p_lightVP );
            // Shadows stay double-sided to avoid holes in thin foliage / two-sided props.
            vkCmdSetCullMode( p_cmd, VK_CULL_MODE_NONE );

            for ( const TDrawCall& dc : p_state.m_drawCalls )
            {
                if ( !dc.m_castShadow ) continue;
                if ( dc.m_mesh == nullptr || dc.m_mesh->m_position == nullptr ) continue;
                if ( dc.m_mesh->m_drawCount == 0 || dc.m_instanceCount == 0 ) continue;

                const bool skinned = dc.m_mesh->isSkinned() && dc.m_mesh->m_joints && dc.m_mesh->m_weights;
                vkCmdBindPipeline( p_cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, skinned ? m_skinnedShadowPipeline : m_shadowPipeline );

                if ( skinned )
                {
                    const VkBuffer     bufs[]    = { dc.m_mesh->m_position->handle(), dc.m_mesh->m_joints->handle(), dc.m_mesh->m_weights->handle() };
                    const VkDeviceSize offsets[] = { 0, 0, 0 };
                    vkCmdBindVertexBuffers( p_cmd, 0, 3, bufs, offsets );
                }
                else
                {
                    const VkBuffer     posBuf = dc.m_mesh->m_position->handle();
                    const VkDeviceSize offset = 0;
                    vkCmdBindVertexBuffers( p_cmd, 0, 1, &posBuf, &offset );
                }

                if ( dc.m_mesh->isIndexed() )
                {
                    const VkIndexType indexType = dc.m_mesh->m_is16BitIndex ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32;
                    vkCmdBindIndexBuffer( p_cmd, dc.m_mesh->m_index->handle(), 0, indexType );
                    vkCmdDrawIndexed( p_cmd, dc.m_mesh->m_drawCount, dc.m_instanceCount, 0, 0, dc.m_instanceOffset );
                }
                else
                {
                    vkCmdDraw( p_cmd, dc.m_mesh->m_drawCount, dc.m_instanceCount, 0, dc.m_instanceOffset );
                }
            }
        };

        auto renderLayer = [ & ]( int32_t p_layer, const glm::mat4& p_lightVP )
        {
            if ( p_layer < 0 || p_layer >= static_cast<int32_t>( g_kMaxShadowMaps ) ) return;

            VkRenderingAttachmentInfo depthAtt{};
            depthAtt.sType                   = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
            depthAtt.imageView               = m_shadowLayerViews[ static_cast<size_t>( p_layer ) ];
            depthAtt.imageLayout             = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
            depthAtt.loadOp                  = VK_ATTACHMENT_LOAD_OP_CLEAR;
            depthAtt.storeOp                 = VK_ATTACHMENT_STORE_OP_STORE;
            depthAtt.clearValue.depthStencil = { 1.0f, 0 };

            VkRenderingInfo rendering{};
            rendering.sType             = VK_STRUCTURE_TYPE_RENDERING_INFO;
            rendering.renderArea.extent = { g_kShadowMapSize, g_kShadowMapSize };
            rendering.layerCount        = 1;
            rendering.pDepthAttachment  = &depthAtt;

            vkCmdBeginRendering( p_cmd, &rendering );
            vkCmdSetViewport( p_cmd, 0, 1, &viewport );
            vkCmdSetScissor( p_cmd, 0, 1, &scissor );
            drawCasters( p_lightVP );
            vkCmdEndRendering( p_cmd );
        };

        for ( const TLightData& light : p_state.m_lights )
        {
            if ( light.m_shadowMap < 0 ) continue;

            if ( light.m_type == 0 )  // point — six cubemap faces into consecutive layers
            {
                for ( uint32_t face = 0; face < g_kPointShadowFaces; ++face )
                {
                    const glm::mat4 faceVP = pointShadowFaceVP( light.m_position, light.m_maxRange, face );
                    renderLayer( light.m_shadowMap + static_cast<int32_t>( face ), faceVP );
                }
            }
            else
            {
                renderLayer( light.m_shadowMap, light.m_vp );
            }
        }

        VkUtil::imageBarrier( p_cmd, m_shadowMaps.handle(), VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                              VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
                              VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                              VK_IMAGE_ASPECT_DEPTH_BIT, g_kMaxShadowMaps );
    }

    void TVkClusteredRenderer::recordClusterCull( VkCommandBuffer p_cmd, const TFrameResources& p_frame )
    {
        vkCmdBindPipeline( p_cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_cullPipeline );
        vkCmdBindDescriptorSets( p_cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_cullPipeLayout, 0, 1, &p_frame.m_cullSet, 0, nullptr );
        vkCmdDispatch( p_cmd, ( g_kClusterCount + 63 ) / 64, 1, 1 );

        VkUtil::memoryBarrier( p_cmd, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                               VK_ACCESS_2_SHADER_STORAGE_READ_BIT );
    }

    void TVkClusteredRenderer::recordForwardPass( VkCommandBuffer p_cmd, const TFrameState& p_state, const TFrameResources& p_frame )
    {
        TOMOS_PROFILE_GPU_BEGIN( m_gpuTimestamps, p_cmd, m_tsFrameIndex, ForwardOpaque );

        const VkExtent2D extent = m_renderExtent;

        VkUtil::imageBarrier( p_cmd, m_hdrA.handle(), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
                              VK_ACCESS_2_NONE, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT );

        VkUtil::imageBarrier( p_cmd, m_depth.handle(), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
                              VK_ACCESS_2_NONE, VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT, VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                              VK_IMAGE_ASPECT_DEPTH_BIT );

        VkRenderingAttachmentInfo colorAtt{};
        colorAtt.sType       = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        colorAtt.imageView   = m_hdrA.view();
        colorAtt.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        colorAtt.loadOp      = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAtt.storeOp     = VK_ATTACHMENT_STORE_OP_STORE;
        colorAtt.clearValue  = { { { 0.02f, 0.02f, 0.03f, 1.0f } } };

        VkRenderingAttachmentInfo depthAtt{};
        depthAtt.sType                   = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        depthAtt.imageView               = m_depth.view();
        depthAtt.imageLayout             = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
        depthAtt.loadOp                  = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depthAtt.storeOp                 = VK_ATTACHMENT_STORE_OP_STORE;
        depthAtt.clearValue.depthStencil = { 1.0f, 0 };

        VkRenderingInfo rendering{};
        rendering.sType                = VK_STRUCTURE_TYPE_RENDERING_INFO;
        rendering.renderArea.extent    = extent;
        rendering.layerCount           = 1;
        rendering.colorAttachmentCount = 1;
        rendering.pColorAttachments    = &colorAtt;
        rendering.pDepthAttachment     = &depthAtt;

        vkCmdBeginRendering( p_cmd, &rendering );

        VkViewport viewport{};
        viewport.width    = static_cast<float>( extent.width );
        viewport.height   = static_cast<float>( extent.height );
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport( p_cmd, 0, 1, &viewport );

        VkRect2D scissor{};
        scissor.extent = extent;
        vkCmdSetScissor( p_cmd, 0, 1, &scissor );

        vkCmdBindDescriptorSets( p_cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_forwardPipeLayout, 0, 1, &p_frame.m_sceneSet, 0, nullptr );

        auto drawMesh = [ & ]( const TDrawCall& p_dc, bool p_blend )
        {
            if ( !p_dc.m_visible ) return;
            if ( p_dc.m_mesh == nullptr || p_dc.m_material == nullptr ) return;
            if ( p_dc.m_mesh->m_position == nullptr || p_dc.m_mesh->m_texCoord == nullptr || p_dc.m_mesh->m_normal == nullptr ||
                 p_dc.m_mesh->m_tangent == nullptr )
                return;
            if ( p_dc.m_mesh->m_drawCount == 0 || p_dc.m_instanceCount == 0 ) return;

            const bool skinned =
                    p_dc.m_mesh->isSkinned() && p_dc.m_mesh->m_joints && p_dc.m_mesh->m_weights && p_state.m_instances[ p_dc.m_instanceOffset ].m_boneCount > 0;

            const auto techIdx = static_cast<size_t>( p_dc.m_material->technique() );
            if ( techIdx >= m_meshTechniques.size() ) return;
            const VkPipeline pipe = m_meshTechniques[ techIdx ].pick( p_blend, skinned );
            if ( pipe == VK_NULL_HANDLE ) return;
            vkCmdBindPipeline( p_cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe );

            // Single-sided materials cull back faces; double-sided draw both
            // (forward.frag still flips normals toward the camera for those).
            vkCmdSetCullMode( p_cmd, p_dc.m_material->doubleSided() ? VK_CULL_MODE_NONE : VK_CULL_MODE_BACK_BIT );

            const VkDescriptorSet matSet = p_dc.m_material->descriptorSet();
            vkCmdBindDescriptorSets( p_cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_forwardPipeLayout, 1, 1, &matSet, 0, nullptr );

            if ( skinned )
            {
                const VkBuffer     vertexBufs[] = { p_dc.m_mesh->m_position->handle(), p_dc.m_mesh->m_texCoord->handle(), p_dc.m_mesh->m_normal->handle(),
                                                    p_dc.m_mesh->m_tangent->handle(),  p_dc.m_mesh->m_joints->handle(),   p_dc.m_mesh->m_weights->handle() };
                const VkDeviceSize offsets[]    = { 0, 0, 0, 0, 0, 0 };
                vkCmdBindVertexBuffers( p_cmd, 0, 6, vertexBufs, offsets );
            }
            else
            {
                const VkBuffer     vertexBufs[] = { p_dc.m_mesh->m_position->handle(), p_dc.m_mesh->m_texCoord->handle(), p_dc.m_mesh->m_normal->handle(),
                                                    p_dc.m_mesh->m_tangent->handle() };
                const VkDeviceSize offsets[]    = { 0, 0, 0, 0 };
                vkCmdBindVertexBuffers( p_cmd, 0, 4, vertexBufs, offsets );
            }

            if ( p_dc.m_mesh->isIndexed() )
            {
                const VkIndexType indexType = p_dc.m_mesh->m_is16BitIndex ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32;
                vkCmdBindIndexBuffer( p_cmd, p_dc.m_mesh->m_index->handle(), 0, indexType );
                vkCmdDrawIndexed( p_cmd, p_dc.m_mesh->m_drawCount, p_dc.m_instanceCount, 0, 0, p_dc.m_instanceOffset );
            }
            else
            {
                vkCmdDraw( p_cmd, p_dc.m_mesh->m_drawCount, p_dc.m_instanceCount, 0, p_dc.m_instanceOffset );
            }
        };

        for ( const TDrawCall& dc : p_state.m_drawCalls )
        {
            if ( dc.m_material == nullptr || dc.m_material->alphaMode() == TMatAlpha::Blend ) continue;
            drawMesh( dc, false );
        }

        // Cutout sprites with opaque; rebind scene set (sprite layout ≠ forward set 0).
        recordSprites( p_cmd, p_state, p_frame, false );
        vkCmdBindDescriptorSets( p_cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_forwardPipeLayout, 0, 1, &p_frame.m_sceneSet, 0, nullptr );

        TOMOS_PROFILE_GPU_END( m_gpuTimestamps, p_cmd, m_tsFrameIndex, ForwardOpaque );
        TOMOS_PROFILE_GPU_BEGIN( m_gpuTimestamps, p_cmd, m_tsFrameIndex, ForwardBlend );

        struct TBlendEntry
        {
            const TDrawCall* m_dc;
            float            m_distSq;
        };

        TArena&                   arena = TFrameAllocator::get().arena();
        TArenaVector<TBlendEntry> blendDraws{ TArenaAllocator<TBlendEntry>( arena ) };
        blendDraws.reserve( p_state.m_drawCalls.size() );

        const glm::vec3 camPos = glm::vec3( p_state.m_viewInv[ 3 ] );
        for ( const TDrawCall& dc : p_state.m_drawCalls )
        {
            if ( !dc.m_visible || dc.m_material == nullptr ) continue;
            if ( dc.m_material->alphaMode() != TMatAlpha::Blend ) continue;

            const TInstanceData& inst = p_state.m_instances[ dc.m_instanceOffset ];
            float                distSq;
            if ( dc.m_mesh != nullptr && dc.m_mesh->m_aabb.valid() )
            {
                const TAABB worldAabb = dc.m_mesh->m_aabb.transformed( inst.m_transform );
                distSq                = 0.0f;
                for ( const glm::vec3& c : worldAabb.corners() )
                {
                    const glm::vec3 d = c - camPos;
                    distSq            = glm::max( distSq, glm::dot( d, d ) );
                }
            }
            else
            {
                const glm::vec3 p = glm::vec3( inst.m_transform[ 3 ] );
                const glm::vec3 d = p - camPos;
                distSq            = glm::dot( d, d );
            }

            blendDraws.push_back( { &dc, distSq } );
        }

        std::sort( blendDraws.begin(), blendDraws.end(), []( const TBlendEntry& p_a, const TBlendEntry& p_b ) { return p_a.m_distSq > p_b.m_distSq; } );

        for ( const TBlendEntry& e : blendDraws ) drawMesh( *e.m_dc, true );

        // Blend sprites after blend meshes (not interleaved).
        recordSprites( p_cmd, p_state, p_frame, true );

        vkCmdEndRendering( p_cmd );

        TOMOS_PROFILE_GPU_END( m_gpuTimestamps, p_cmd, m_tsFrameIndex, ForwardBlend );
    }

    void TVkClusteredRenderer::recordPost( VkCommandBuffer p_cmd, uint32_t p_frameIndex, const TFrameState& p_state )
    {
        VkUtil::imageBarrier( p_cmd, m_hdrA.handle(), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                              VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                              VK_ACCESS_2_SHADER_SAMPLED_READ_BIT );

        VkUtil::imageBarrier( p_cmd, m_depth.handle(), VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                              VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT, VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                              VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_SAMPLED_READ_BIT, VK_IMAGE_ASPECT_DEPTH_BIT );

        const TRenderDestination dest = m_gpu.resolvedRenderDestination();

        TPostContext ctx{};
        ctx.m_gpu          = &m_gpu;
        ctx.m_extent       = dest.m_extent;
        ctx.m_hdr          = &m_hdrA;
        ctx.m_hdrOther     = &m_hdrB;
        ctx.m_depth        = &m_depth;
        ctx.m_near         = p_state.m_near;
        ctx.m_far          = p_state.m_far;
        ctx.m_proj         = p_state.m_proj;
        ctx.m_projInv      = p_state.m_projInv;
        ctx.m_viewInv      = p_state.m_viewInv;
        ctx.m_outputFormat = dest.m_format;
        ctx.m_frameIndex   = p_frameIndex;
        ctx.m_outputImage  = dest.m_image;
        ctx.m_outputView   = dest.m_view;
#if TOMOS_DEBUG
        ctx.m_gpuTimestamps = &m_gpuTimestamps;
#endif

        m_post.execute( p_cmd, ctx );

        if ( dest.m_sampleAfterTonemap && dest.m_image != VK_NULL_HANDLE )
        {
            VkUtil::imageBarrier( p_cmd, dest.m_image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                  VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                                  VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_SAMPLED_READ_BIT );
        }
    }

    void TVkClusteredRenderer::recordSprites( VkCommandBuffer p_cmd, const TFrameState& p_state, const TFrameResources& p_frame, bool p_blend )
    {
        bool any = false;
        for ( const TSpriteBatch& batch : p_state.m_spriteBatches )
            if ( batch.m_blend == p_blend && batch.m_count > 0 ) any = true;
        if ( !any ) return;

        vkCmdBindPipeline( p_cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, p_blend ? m_spriteBlendPipeline : m_spriteCutoutPipeline );
        vkCmdSetCullMode( p_cmd, VK_CULL_MODE_NONE );
        vkCmdBindDescriptorSets( p_cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_spritePipeLayout, 0, 1, &p_frame.m_spriteSet, 0, nullptr );

        for ( const TSpriteBatch& batch : p_state.m_spriteBatches )
        {
            if ( batch.m_blend != p_blend || batch.m_count == 0 ) continue;

            const VkDescriptorSet texSet = spriteTextureSet( batch.m_texture );
            vkCmdBindDescriptorSets( p_cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_spritePipeLayout, 1, 1, &texSet, 0, nullptr );

            vkCmdDraw( p_cmd, 6, batch.m_count, 0, batch.m_offset );
        }
    }

    void TVkClusteredRenderer::recordParticleSim( VkCommandBuffer p_cmd, const TFrameResources& p_frame, uint32_t p_emitterCount )
    {
        vkCmdFillBuffer( p_cmd, m_particleCounters.handle(), offsetof( TParticleCounters, m_aliveCount ), sizeof( uint32_t ), 0 );
        vkCmdFillBuffer( p_cmd, m_texBucketBuf.handle(), offsetof( TParticleTexBuckets, m_counts ), sizeof( TParticleTexBuckets::m_counts ), 0 );

        VkUtil::memoryBarrier( p_cmd, VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                               VK_ACCESS_2_SHADER_STORAGE_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT );

        vkCmdBindPipeline( p_cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_particleSimPipeline );
        vkCmdBindDescriptorSets( p_cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_particleSimPipeLayout, 0, 1, &p_frame.m_particleSimSet, 0, nullptr );

        auto dispatchPhase = [ & ]( uint32_t p_phase, uint32_t p_groupCount )
        {
            vkCmdPushConstants( p_cmd, m_particleSimPipeLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof( uint32_t ), &p_phase );
            vkCmdDispatch( p_cmd, p_groupCount, 1, 1 );
            VkUtil::memoryBarrier( p_cmd, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                                   VK_ACCESS_2_SHADER_STORAGE_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT );
        };

        dispatchPhase( 0, ( g_kMaxParticles + 63 ) / 64 );

        if ( p_emitterCount > 0 ) dispatchPhase( 1, ( p_emitterCount + 63 ) / 64 );

        // Bucket the alive list per texture, then emit one draw command per slot.
        dispatchPhase( 2, 1 );
        dispatchPhase( 3, ( g_kMaxParticles + 63 ) / 64 );
        dispatchPhase( 4, ( g_kMaxParticleTextures + 63 ) / 64 );

        VkUtil::memoryBarrier( p_cmd, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                               VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT,
                               VK_ACCESS_2_SHADER_STORAGE_READ_BIT | VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT );
    }

    void TVkClusteredRenderer::recordParticlePass( VkCommandBuffer p_cmd, const TFrameState& p_state, const TFrameResources& p_frame )
    {
        const VkExtent2D extent = m_renderExtent;

        VkUtil::imageBarrier( p_cmd, m_hdrA.handle(), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                              VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                              VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT );
        VkUtil::imageBarrier( p_cmd, m_depth.handle(), VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
                              VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT, VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                              VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
                              VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_IMAGE_ASPECT_DEPTH_BIT );

        VkRenderingAttachmentInfo colorAtt{};
        colorAtt.sType       = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        colorAtt.imageView   = m_hdrA.view();
        colorAtt.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        colorAtt.loadOp      = VK_ATTACHMENT_LOAD_OP_LOAD;
        colorAtt.storeOp     = VK_ATTACHMENT_STORE_OP_STORE;

        VkRenderingAttachmentInfo depthAtt{};
        depthAtt.sType       = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        depthAtt.imageView   = m_depth.view();
        depthAtt.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
        depthAtt.loadOp      = VK_ATTACHMENT_LOAD_OP_LOAD;
        depthAtt.storeOp     = VK_ATTACHMENT_STORE_OP_STORE;

        VkRenderingInfo rendering{};
        rendering.sType                = VK_STRUCTURE_TYPE_RENDERING_INFO;
        rendering.renderArea.extent    = extent;
        rendering.layerCount           = 1;
        rendering.colorAttachmentCount = 1;
        rendering.pColorAttachments    = &colorAtt;
        rendering.pDepthAttachment     = &depthAtt;

        vkCmdBeginRendering( p_cmd, &rendering );

        VkViewport viewport{};
        viewport.width    = static_cast<float>( extent.width );
        viewport.height   = static_cast<float>( extent.height );
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        VkRect2D scissor{};
        scissor.extent = extent;
        vkCmdSetViewport( p_cmd, 0, 1, &viewport );
        vkCmdSetScissor( p_cmd, 0, 1, &scissor );
        vkCmdSetCullMode( p_cmd, VK_CULL_MODE_NONE );

        vkCmdBindPipeline( p_cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_particlePipeline );
        vkCmdBindDescriptorSets( p_cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_particleDrawPipeLayout, 0, 1, &p_frame.m_particleDrawSet, 0, nullptr );

        // One indirect draw per allocated texture slot; the sim wrote each slot's
        // instanceCount, so empty slots cost nothing and no particle is drawn twice.
        const size_t texCount = std::min<size_t>( std::max<size_t>( 1, p_state.m_particleTextures.size() ), g_kMaxParticleTextures );
        for ( size_t i = 0; i < texCount; ++i )
        {
            const TVkImage*       tex    = ( i < p_state.m_particleTextures.size() ) ? p_state.m_particleTextures[ i ] : nullptr;
            const VkDescriptorSet texSet = spriteTextureSet( tex );
            vkCmdBindDescriptorSets( p_cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_particleDrawPipeLayout, 1, 1, &texSet, 0, nullptr );

            const auto texIndex = static_cast<uint32_t>( i );
            vkCmdPushConstants( p_cmd, m_particleDrawPipeLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof( uint32_t ), &texIndex );

            vkCmdDrawIndirect( p_cmd, m_texDrawCmdBuf.handle(), i * sizeof( TParticleTexDrawCmd ), 1, sizeof( TParticleTexDrawCmd ) );
        }

        vkCmdEndRendering( p_cmd );
    }

    VkDescriptorSet TVkClusteredRenderer::spriteTextureSet( const TVkImage* p_texture )
    {
        const TVkImage* texture = ( p_texture != nullptr && p_texture->valid() ) ? p_texture : &m_gpu.defaultTexture();

        const auto it = m_spriteTexSets.find( texture );
        if ( it != m_spriteTexSets.end() ) return it->second;

        VkDescriptorSetAllocateInfo ai{};
        ai.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        ai.descriptorPool     = m_gpu.descPool();
        ai.descriptorSetCount = 1;
        ai.pSetLayouts        = &m_spriteTexLayout;

        VkDescriptorSet set = VK_NULL_HANDLE;
        if ( vkAllocateDescriptorSets( m_gpu.device(), &ai, &set ) != VK_SUCCESS )
            throw std::runtime_error( "[TVkClusteredRenderer] Failed to allocate sprite texture set" );

        VkDescriptorImageInfo imgInfo{ texture->sampler(), texture->view(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
        VkWriteDescriptorSet  write{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,    nullptr,  set,     0,      0, 1,
                                    VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &imgInfo, nullptr, nullptr };
        vkUpdateDescriptorSets( m_gpu.device(), 1, &write, 0, nullptr );

        m_spriteTexSets.emplace( texture, set );
        return set;
    }
}  // namespace Tomos
