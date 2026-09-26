#include "Tomos/gpu/vulkan/post/TPostSAO.hh"

#include <algorithm>
#include <glm/glm.hpp>

#include "Tomos/gpu/vulkan/TVkGpu.hh"
#include "Tomos/gpu/vulkan/TVkUtil.hh"
#include "Tomos/gpu/vulkan/post/TPostUtil.hh"

namespace Tomos
{
    namespace
    {
        struct TLinPC
        {
            float m_near;
            float m_far;
            float m_pad[ 2 ];
        };

        struct TSamplePC
        {
            glm::mat4 m_proj;
            glm::vec4 m_screenSizeRadius;
            glm::vec4 m_params;
        };

        struct TBlurPC
        {
            glm::vec2 m_texelSize;
            float     m_horizontal;
            float     m_sharpness;
        };

        struct TCompPC
        {
            float m_intensity;
            float m_pad[ 3 ];
        };

        struct TTemporalPC
        {
            float m_blend;  // history weight
            float m_pad[ 3 ];
        };
    }  // namespace

    void TPostSAO::ensurePipelines( const TPostContext& p_ctx )
    {
        if ( m_linPipe != VK_NULL_HANDLE ) return;

        m_device = p_ctx.m_gpu->device();

        VkDescriptorSetLayoutBinding    b0{ 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr };
        VkDescriptorSetLayoutCreateInfo li1{};
        li1.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        li1.bindingCount = 1;
        li1.pBindings    = &b0;
        vkCreateDescriptorSetLayout( m_device, &li1, nullptr, &m_samp1Layout );

        VkDescriptorSetLayoutBinding b2[ 2 ] = {
                { 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr },
                { 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr },
        };
        VkDescriptorSetLayoutCreateInfo li2{};
        li2.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        li2.bindingCount = 2;
        li2.pBindings    = b2;
        vkCreateDescriptorSetLayout( m_device, &li2, nullptr, &m_samp2Layout );

        auto makeLay = [ & ]( VkDescriptorSetLayout p_setLay, uint32_t p_pcSize, VkPipelineLayout* p_out )
        {
            VkPushConstantRange pc{};
            pc.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
            pc.size       = p_pcSize;
            VkPipelineLayoutCreateInfo pl{};
            pl.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
            pl.setLayoutCount         = 1;
            pl.pSetLayouts            = &p_setLay;
            pl.pushConstantRangeCount = p_pcSize ? 1u : 0u;
            pl.pPushConstantRanges    = p_pcSize ? &pc : nullptr;
            vkCreatePipelineLayout( m_device, &pl, nullptr, p_out );
        };

        const VkFormat aoFmt = VK_FORMAT_R32_SFLOAT;

        makeLay( m_samp1Layout, sizeof( TLinPC ), &m_linLay );
        makeLay( m_samp1Layout, sizeof( TSamplePC ), &m_sampleLay );
        makeLay( m_samp2Layout, sizeof( TBlurPC ), &m_blurLay );
        makeLay( m_samp2Layout, sizeof( TTemporalPC ), &m_temporalLay );
        makeLay( m_samp2Layout, sizeof( TCompPC ), &m_compLay );

        m_linPipe       = PostUtil::createFullscreenPipeline( m_device, m_linLay, "sao_linearize.frag.spv", VK_FORMAT_R32_SFLOAT );
        m_samplePipe    = PostUtil::createFullscreenPipeline( m_device, m_sampleLay, "sao_sample.frag.spv", aoFmt );
        m_blurPipe      = PostUtil::createFullscreenPipeline( m_device, m_blurLay, "sao_blur.frag.spv", aoFmt );
        m_temporalPipe  = PostUtil::createFullscreenPipeline( m_device, m_temporalLay, "sao_temporal.frag.spv", aoFmt );
        m_compPipe      = PostUtil::createFullscreenPipeline( m_device, m_compLay, "sao_compose.frag.spv", VK_FORMAT_R16G16B16A16_SFLOAT );

        for ( uint32_t i = 0; i < g_kFrames; ++i )
        {
            m_linSets[ i ]       = PostUtil::allocSet( m_device, p_ctx.m_gpu->descPool(), m_samp1Layout );
            m_sampleSets[ i ]    = PostUtil::allocSet( m_device, p_ctx.m_gpu->descPool(), m_samp1Layout );
            m_blurHSets[ i ]     = PostUtil::allocSet( m_device, p_ctx.m_gpu->descPool(), m_samp2Layout );
            m_blurVSets[ i ]     = PostUtil::allocSet( m_device, p_ctx.m_gpu->descPool(), m_samp2Layout );
            m_blurHSets2[ i ]    = PostUtil::allocSet( m_device, p_ctx.m_gpu->descPool(), m_samp2Layout );
            m_blurVSets2[ i ]    = PostUtil::allocSet( m_device, p_ctx.m_gpu->descPool(), m_samp2Layout );
            m_temporalSets[ i ]  = PostUtil::allocSet( m_device, p_ctx.m_gpu->descPool(), m_samp2Layout );
            m_compSets[ i ]      = PostUtil::allocSet( m_device, p_ctx.m_gpu->descPool(), m_samp2Layout );
        }
    }

    void TPostSAO::onResize( const TPostContext& p_ctx )
    {
        ensurePipelines( p_ctx );

        const VkExtent2D extent = p_ctx.m_extent;

        const TVkImageDesc zDesc{ extent.width,       extent.height,   1,    1,    TImgFormat::R32Float, TImgUsage::ColorAttachment | TImgUsage::Sampled,
                                  TTexFilter::Nearest, TTexAddr::Clamp, true, false };
        m_linearZ = TVkImage{};
        m_linearZ = TVkImage( p_ctx.m_gpu->device(), p_ctx.m_gpu->physDevice(), zDesc );

        const TVkImageDesc aoDesc{ extent.width,      extent.height,   1,    1,    TImgFormat::R32Float,
                                   TImgUsage::ColorAttachment | TImgUsage::Sampled | TImgUsage::CopySrc | TImgUsage::CopyDst,
                                   TTexFilter::Linear, TTexAddr::Clamp, true, false };
        m_aoPacked = TVkImage{};
        m_aoBlurA  = TVkImage{};
        m_aoBlurB  = TVkImage{};
        m_aoHist   = TVkImage{};
        m_aoPacked = TVkImage( p_ctx.m_gpu->device(), p_ctx.m_gpu->physDevice(), aoDesc );
        m_aoBlurA  = TVkImage( p_ctx.m_gpu->device(), p_ctx.m_gpu->physDevice(), aoDesc );
        m_aoBlurB  = TVkImage( p_ctx.m_gpu->device(), p_ctx.m_gpu->physDevice(), aoDesc );
        m_aoHist   = TVkImage( p_ctx.m_gpu->device(), p_ctx.m_gpu->physDevice(), aoDesc );
        m_histValid = false;
    }

    void TPostSAO::record( VkCommandBuffer p_cmd, TPostContext& p_ctx )
    {
        const uint32_t   fi     = p_ctx.m_frameIndex % g_kFrames;
        const VkExtent2D extent = p_ctx.m_extent;

        auto barrierToColor = [ & ]( TVkImage& p_img )
        {
            VkUtil::imageBarrier( p_cmd, p_img.handle(), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                  VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT, VK_ACCESS_2_NONE, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                                  VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT );
        };
        auto barrierToSample = [ & ]( TVkImage& p_img )
        {
            VkUtil::imageBarrier( p_cmd, p_img.handle(), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                  VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                                  VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_SAMPLED_READ_BIT );
        };

        // 1. Linearize depth → camera-space Z.
        barrierToColor( m_linearZ );
        PostUtil::writeCombinedImage( m_device, m_linSets[ fi ], 0, *p_ctx.m_depth );
        TLinPC linPc{ p_ctx.m_near, p_ctx.m_far, { 0, 0 } };

        PostUtil::beginColorPass( p_cmd, m_linearZ.view(), extent );
        vkCmdBindPipeline( p_cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_linPipe );
        vkCmdBindDescriptorSets( p_cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_linLay, 0, 1, &m_linSets[ fi ], 0, nullptr );
        vkCmdPushConstants( p_cmd, m_linLay, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof( linPc ), &linPc );
        vkCmdDraw( p_cmd, 3, 1, 0, 0 );
        vkCmdEndRendering( p_cmd );
        barrierToSample( m_linearZ );

        // 2. Alchemy sample pass → raw AO (R32F).
        barrierToColor( m_aoPacked );
        PostUtil::writeCombinedImage( m_device, m_sampleSets[ fi ], 0, m_linearZ );
        TSamplePC samplePc{};
        samplePc.m_proj             = p_ctx.m_proj;
        samplePc.m_screenSizeRadius = glm::vec4( static_cast<float>( extent.width ), static_cast<float>( extent.height ), m_radius, 0.0f );
        samplePc.m_params           = glm::vec4( m_bias, static_cast<float>( std::max( m_sampleCount, 1 ) ), 0.0f, 0.0f );

        PostUtil::beginColorPass( p_cmd, m_aoPacked.view(), extent );
        vkCmdBindPipeline( p_cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_samplePipe );
        vkCmdBindDescriptorSets( p_cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_sampleLay, 0, 1, &m_sampleSets[ fi ], 0, nullptr );
        vkCmdPushConstants( p_cmd, m_sampleLay, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof( samplePc ), &samplePc );
        vkCmdDraw( p_cmd, 3, 1, 0, 0 );
        vkCmdEndRendering( p_cmd );
        barrierToSample( m_aoPacked );

        // 3–4. Bilateral blur (H then V), twice for extra smoothness.
        auto blurPass = [ & ]( TVkImage& p_src, TVkImage& p_dst, VkDescriptorSet p_set, float p_horizontal )
        {
            barrierToColor( p_dst );
            PostUtil::writeCombinedImage( m_device, p_set, 0, p_src );
            PostUtil::writeCombinedImage( m_device, p_set, 1, m_linearZ );
            TBlurPC bpc{};
            bpc.m_texelSize  = { 1.0f / static_cast<float>( extent.width ), 1.0f / static_cast<float>( extent.height ) };
            bpc.m_horizontal = p_horizontal;
            bpc.m_sharpness  = m_blurSharpness;
            PostUtil::beginColorPass( p_cmd, p_dst.view(), extent );
            vkCmdBindPipeline( p_cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_blurPipe );
            vkCmdBindDescriptorSets( p_cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_blurLay, 0, 1, &p_set, 0, nullptr );
            vkCmdPushConstants( p_cmd, m_blurLay, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof( bpc ), &bpc );
            vkCmdDraw( p_cmd, 3, 1, 0, 0 );
            vkCmdEndRendering( p_cmd );
            barrierToSample( p_dst );
        };
        blurPass( m_aoPacked, m_aoBlurB, m_blurHSets[ fi ], 1.0f );
        blurPass( m_aoBlurB, m_aoBlurA, m_blurVSets[ fi ], 0.0f );
        blurPass( m_aoBlurA, m_aoBlurB, m_blurHSets2[ fi ], 1.0f );
        blurPass( m_aoBlurB, m_aoBlurA, m_blurVSets2[ fi ], 0.0f );

        // 5. Temporal blend with history (reduces per-frame noise when moving).
        barrierToColor( m_aoBlurB );
        PostUtil::writeCombinedImage( m_device, m_temporalSets[ fi ], 0, m_aoBlurA );
        PostUtil::writeCombinedImage( m_device, m_temporalSets[ fi ], 1, m_histValid ? m_aoHist : m_aoBlurA );

        TTemporalPC tpc{ m_histValid ? m_temporalBlend : 0.0f, { 0, 0, 0 } };
        PostUtil::beginColorPass( p_cmd, m_aoBlurB.view(), extent );
        vkCmdBindPipeline( p_cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_temporalPipe );
        vkCmdBindDescriptorSets( p_cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_temporalLay, 0, 1, &m_temporalSets[ fi ], 0, nullptr );
        vkCmdPushConstants( p_cmd, m_temporalLay, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof( tpc ), &tpc );
        vkCmdDraw( p_cmd, 3, 1, 0, 0 );
        vkCmdEndRendering( p_cmd );
        barrierToSample( m_aoBlurB );

        // Copy blended AO to history for next frame.
        VkUtil::imageBarrier( p_cmd, m_aoHist.handle(), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                              VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_SAMPLED_READ_BIT, VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                              VK_ACCESS_2_TRANSFER_WRITE_BIT );
        VkUtil::imageBarrier( p_cmd, m_aoBlurB.handle(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                              VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_SAMPLED_READ_BIT, VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                              VK_ACCESS_2_TRANSFER_READ_BIT );

        VkImageCopy copyRegion{};
        copyRegion.srcSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        copyRegion.srcSubresource.layerCount     = 1;
        copyRegion.dstSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        copyRegion.dstSubresource.layerCount     = 1;
        copyRegion.extent                        = { extent.width, extent.height, 1 };
        vkCmdCopyImage( p_cmd, m_aoBlurB.handle(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, m_aoHist.handle(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
                        &copyRegion );

        VkUtil::imageBarrier( p_cmd, m_aoHist.handle(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                              VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                              VK_ACCESS_2_SHADER_SAMPLED_READ_BIT );
        VkUtil::imageBarrier( p_cmd, m_aoBlurB.handle(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                              VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_READ_BIT, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                              VK_ACCESS_2_SHADER_SAMPLED_READ_BIT );
        m_histValid = true;

        // 6. Multiply AO into HDR.
        VkUtil::imageBarrier( p_cmd, p_ctx.m_hdrOther->handle(), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                              VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT, VK_ACCESS_2_NONE, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                              VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT );

        PostUtil::writeCombinedImage( m_device, m_compSets[ fi ], 0, *p_ctx.m_hdr );
        PostUtil::writeCombinedImage( m_device, m_compSets[ fi ], 1, m_aoBlurB );
        TCompPC cpc{ m_intensity, { 0, 0, 0 } };

        PostUtil::beginColorPass( p_cmd, p_ctx.m_hdrOther->view(), extent );
        vkCmdBindPipeline( p_cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_compPipe );
        vkCmdBindDescriptorSets( p_cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_compLay, 0, 1, &m_compSets[ fi ], 0, nullptr );
        vkCmdPushConstants( p_cmd, m_compLay, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof( cpc ), &cpc );
        vkCmdDraw( p_cmd, 3, 1, 0, 0 );
        vkCmdEndRendering( p_cmd );

        VkUtil::imageBarrier( p_cmd, p_ctx.m_hdrOther->handle(), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                              VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                              VK_ACCESS_2_SHADER_SAMPLED_READ_BIT );

        p_ctx.swapHdr();
    }

    void TPostSAO::destroy()
    {
        m_linearZ  = TVkImage{};
        m_aoPacked = TVkImage{};
        m_aoBlurA  = TVkImage{};
        m_aoBlurB  = TVkImage{};
        m_aoHist   = TVkImage{};
        m_histValid = false;
        if ( m_device == VK_NULL_HANDLE ) return;
        vkDestroyPipeline( m_device, m_linPipe, nullptr );
        vkDestroyPipeline( m_device, m_samplePipe, nullptr );
        vkDestroyPipeline( m_device, m_blurPipe, nullptr );
        vkDestroyPipeline( m_device, m_temporalPipe, nullptr );
        vkDestroyPipeline( m_device, m_compPipe, nullptr );
        vkDestroyPipelineLayout( m_device, m_linLay, nullptr );
        vkDestroyPipelineLayout( m_device, m_sampleLay, nullptr );
        vkDestroyPipelineLayout( m_device, m_blurLay, nullptr );
        vkDestroyPipelineLayout( m_device, m_temporalLay, nullptr );
        vkDestroyPipelineLayout( m_device, m_compLay, nullptr );
        vkDestroyDescriptorSetLayout( m_device, m_samp1Layout, nullptr );
        vkDestroyDescriptorSetLayout( m_device, m_samp2Layout, nullptr );
        m_linPipe = m_samplePipe = m_blurPipe = m_temporalPipe = m_compPipe = VK_NULL_HANDLE;
        m_device                                                              = VK_NULL_HANDLE;
    }
}  // namespace Tomos
