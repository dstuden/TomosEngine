#define GLFW_INCLUDE_VULKAN
#include "Tomos/gpu/vulkan/TVkGpu.hh"

#include <algorithm>
#include <stdexcept>
#include <vector>

#include "Tomos/gpu/TRenderLimits.hh"
#include "Tomos/gpu/vulkan/TVkPass.hh"
#include "Tomos/gpu/vulkan/TVkUtil.hh"
#include "Tomos/gpu/vulkan/renderer/TVkClusteredRenderer.hh"

namespace Tomos
{
    void TVkGpu::beginUploadBatch()
    {
        if ( m_batchOpen ) throw std::runtime_error( "[TVkGpu] Upload batch already open" );

        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool        = m_uploadPool;
        allocInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = 1;
        if ( vkAllocateCommandBuffers( m_device, &allocInfo, &m_batchCmd ) != VK_SUCCESS )
            throw std::runtime_error( "[TVkGpu] Failed to allocate upload command buffer" );

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer( m_batchCmd, &beginInfo );

        m_batchStaging.clear();
        m_batchOpen = true;
    }

    void TVkGpu::endUploadBatch()
    {
        if ( !m_batchOpen ) return;

        vkEndCommandBuffer( m_batchCmd );

        if ( vkResetFences( m_device, 1, &m_uploadFence ) != VK_SUCCESS )
            throw std::runtime_error( "[TVkGpu] Failed to reset upload fence" );

        VkSubmitInfo submitInfo{};
        submitInfo.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers    = &m_batchCmd;

        if ( vkQueueSubmit( m_graphicsQueue, 1, &submitInfo, m_uploadFence ) != VK_SUCCESS )
            throw std::runtime_error( "[TVkGpu] Upload batch submit failed" );

        vkWaitForFences( m_device, 1, &m_uploadFence, VK_TRUE, UINT64_MAX );

        vkFreeCommandBuffers( m_device, m_uploadPool, 1, &m_batchCmd );
        m_batchCmd = VK_NULL_HANDLE;
        m_batchStaging.clear();
        m_batchOpen = false;
    }

    void TVkGpu::uploadBuffer( TVkBuffer& p_dst, const void* p_data, size_t p_size )
    {
        const bool ownedBatch = !m_batchOpen;
        if ( ownedBatch ) beginUploadBatch();

        TVkBuffer staging( m_device, m_physDevice, p_size, TBufUsage::Uniform | TBufUsage::CopySrc );
        staging.upload( p_data, 0, p_size );

        VkBufferCopy region{};
        region.size = p_size;
        vkCmdCopyBuffer( m_batchCmd, staging.handle(), p_dst.handle(), 1, &region );

        m_batchStaging.push_back( std::move( staging ) );

        if ( ownedBatch ) endUploadBatch();
    }

    void TVkGpu::uploadImage( TVkImage& p_dst, const void* p_pixels, uint32_t p_width, uint32_t p_height )
    {
        const bool ownedBatch = !m_batchOpen;
        if ( ownedBatch ) beginUploadBatch();

        const size_t   byteSize  = static_cast<size_t>( p_width ) * p_height * 4;
        const uint32_t mipLevels = std::max( 1u, p_dst.mipLevels() );

        TVkBuffer staging( m_device, m_physDevice, byteSize, TBufUsage::Uniform | TBufUsage::CopySrc );
        staging.upload( p_pixels, 0, byteSize );

        VkCommandBuffer p_cmd = m_batchCmd;

        // Transition only mip 0 for the buffer copy; remaining levels stay UNDEFINED until blit.
        VkUtil::imageBarrier( p_cmd, p_dst.handle(), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_2_NONE,
                              VK_ACCESS_2_NONE, VK_PIPELINE_STAGE_2_COPY_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_IMAGE_ASPECT_COLOR_BIT, p_dst.layers(), 1 );

        VkBufferImageCopy region{};
        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.layerCount = 1;
        region.imageExtent                 = { p_width, p_height, 1 };
        vkCmdCopyBufferToImage( p_cmd, staging.handle(), p_dst.handle(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region );

        if ( mipLevels == 1 )
        {
            VkUtil::imageBarrier( p_cmd, p_dst.handle(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                  VK_PIPELINE_STAGE_2_COPY_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                                  VK_ACCESS_2_SHADER_READ_BIT );
        }
        else
        {
            // Generate the rest of the chain with linear blits (requires CopySrc|CopyDst usage).
            auto mipBarrier = [ & ]( uint32_t p_mip, VkImageLayout p_old, VkImageLayout p_new, VkPipelineStageFlags2 p_srcStage, VkAccessFlags2 p_srcAccess,
                                     VkPipelineStageFlags2 p_dstStage, VkAccessFlags2 p_dstAccess )
            {
                VkImageMemoryBarrier2 barrier{};
                barrier.sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
                barrier.srcStageMask                    = p_srcStage;
                barrier.srcAccessMask                   = p_srcAccess;
                barrier.dstStageMask                    = p_dstStage;
                barrier.dstAccessMask                   = p_dstAccess;
                barrier.oldLayout                       = p_old;
                barrier.newLayout                       = p_new;
                barrier.image                           = p_dst.handle();
                barrier.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
                barrier.subresourceRange.baseMipLevel   = p_mip;
                barrier.subresourceRange.levelCount     = 1;
                barrier.subresourceRange.baseArrayLayer = 0;
                barrier.subresourceRange.layerCount     = 1;

                VkDependencyInfo dep{};
                dep.sType                   = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
                dep.imageMemoryBarrierCount = 1;
                dep.pImageMemoryBarriers    = &barrier;
                vkCmdPipelineBarrier2( p_cmd, &dep );
            };

            auto mipW = static_cast<int32_t>( p_width );
            auto mipH = static_cast<int32_t>( p_height );

            for ( uint32_t level = 1; level < mipLevels; ++level )
            {
                const VkPipelineStageFlags2 srcWriteStage = ( level == 1 ) ? VK_PIPELINE_STAGE_2_COPY_BIT : VK_PIPELINE_STAGE_2_BLIT_BIT;

                mipBarrier( level - 1, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, srcWriteStage,
                            VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_2_BLIT_BIT, VK_ACCESS_2_TRANSFER_READ_BIT );

                // Destination mip starts UNDEFINED → TRANSFER_DST.
                mipBarrier( level, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_2_NONE, VK_ACCESS_2_NONE,
                            VK_PIPELINE_STAGE_2_BLIT_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT );

                const int32_t nextW = mipW > 1 ? mipW / 2 : 1;
                const int32_t nextH = mipH > 1 ? mipH / 2 : 1;

                VkImageBlit blit{};
                blit.srcOffsets[ 0 ]               = { 0, 0, 0 };
                blit.srcOffsets[ 1 ]               = { mipW, mipH, 1 };
                blit.srcSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
                blit.srcSubresource.mipLevel       = level - 1;
                blit.srcSubresource.baseArrayLayer = 0;
                blit.srcSubresource.layerCount     = 1;
                blit.dstOffsets[ 0 ]               = { 0, 0, 0 };
                blit.dstOffsets[ 1 ]               = { nextW, nextH, 1 };
                blit.dstSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
                blit.dstSubresource.mipLevel       = level;
                blit.dstSubresource.baseArrayLayer = 0;
                blit.dstSubresource.layerCount     = 1;

                vkCmdBlitImage( p_cmd, p_dst.handle(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, p_dst.handle(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit,
                                VK_FILTER_LINEAR );

                mipBarrier( level - 1, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_2_BLIT_BIT,
                            VK_ACCESS_2_TRANSFER_READ_BIT, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT );

                mipW = nextW;
                mipH = nextH;
            }

            mipBarrier( mipLevels - 1, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_2_BLIT_BIT,
                        VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT );
        }

        m_batchStaging.push_back( std::move( staging ) );

        if ( ownedBatch ) endUploadBatch();
    }

    void TVkGpu::uploadFrameState( TVkFrameData& p_frame )
    {
        TSceneUBO sceneUbo{};
        sceneUbo.m_viewProj      = m_frameState.m_viewProj;
        sceneUbo.m_view          = m_frameState.m_view;
        sceneUbo.m_viewInv       = m_frameState.m_viewInv;
        sceneUbo.m_projInv       = m_frameState.m_projInv;
        sceneUbo.m_cameraPosNear = glm::vec4( glm::vec3( m_frameState.m_viewInv[ 3 ] ), m_frameState.m_near );
        sceneUbo.m_screenFar     = glm::vec4( static_cast<float>( m_renderExtent.width ), static_cast<float>( m_renderExtent.height ), m_frameState.m_far,
                                              static_cast<float>( m_frameState.m_lights.size() ) );
        sceneUbo.m_clusterGrid   = glm::uvec4( g_kClusterX, g_kClusterY, g_kClusterZ, g_kMaxLightsPerCluster );
        sceneUbo.m_debug         = glm::uvec4( static_cast<uint32_t>( m_frameState.m_debugMode ), 0u, 0u, 0u );
        sceneUbo.m_time          = m_frameState.m_time;
        p_frame.m_sceneUBO.upload( &sceneUbo, 0, sizeof( TSceneUBO ) );

        if ( !m_frameState.m_instances.empty() )
            p_frame.m_instanceBuf.upload( m_frameState.m_instances.data(), 0, m_frameState.m_instances.size() * sizeof( TInstanceData ) );

        if ( !m_frameState.m_lights.empty() ) p_frame.m_lightBuf.upload( m_frameState.m_lights.data(), 0, m_frameState.m_lights.size() * sizeof( TLightData ) );

        if ( !m_frameState.m_sprites.empty() )
            p_frame.m_spriteBuf.upload( m_frameState.m_sprites.data(), 0, m_frameState.m_sprites.size() * sizeof( TSpriteData ) );

        if ( !m_frameState.m_bones.empty() ) p_frame.m_boneBuf.upload( m_frameState.m_bones.data(), 0, m_frameState.m_bones.size() * sizeof( glm::mat4 ) );

        m_renderer->uploadParticles( m_frameIndex, m_frameState );
    }

}  // namespace Tomos
