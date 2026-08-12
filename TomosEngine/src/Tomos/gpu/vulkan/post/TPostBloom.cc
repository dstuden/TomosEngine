#include "Tomos/gpu/vulkan/post/TPostBloom.hh"

#include <glm/glm.hpp>

#include "Tomos/gpu/vulkan/TVkGpu.hh"
#include "Tomos/gpu/vulkan/TVkUtil.hh"
#include "Tomos/gpu/vulkan/post/TPostUtil.hh"

namespace Tomos
{
    namespace
    {
        struct TExtractPC
        {
            float m_threshold;
            float m_pad[ 3 ];
        };
        struct TBlurPC
        {
            glm::vec2 m_texelSize;
            float     m_horizontal;
            float     m_pad;
        };
        struct TCompPC
        {
            float m_strength;
            float m_pad[ 3 ];
        };
    }  // namespace

    void TPostBloom::ensurePipelines( const TPostContext& p_ctx )
    {
        if ( m_extractPipe != VK_NULL_HANDLE ) return;

        m_device              = p_ctx.m_gpu->device();
        const VkFormat hdrFmt = VK_FORMAT_R16G16B16A16_SFLOAT;

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

        auto makeLay = [ & ]( VkDescriptorSetLayout setLay, uint32_t pcSize, VkPipelineLayout* out )
        {
            VkPushConstantRange pc{};
            pc.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
            pc.size       = pcSize;
            VkPipelineLayoutCreateInfo pl{};
            pl.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
            pl.setLayoutCount         = 1;
            pl.pSetLayouts            = &setLay;
            pl.pushConstantRangeCount = pcSize ? 1u : 0u;
            pl.pPushConstantRanges    = pcSize ? &pc : nullptr;
            vkCreatePipelineLayout( m_device, &pl, nullptr, out );
        };

        makeLay( m_samp1Layout, sizeof( TExtractPC ), &m_extractLay );
        makeLay( m_samp1Layout, sizeof( TBlurPC ), &m_blurLay );
        makeLay( m_samp2Layout, sizeof( TCompPC ), &m_compLay );

        m_extractPipe = PostUtil::createFullscreenPipeline( m_device, m_extractLay, "bloom_extract.frag.spv", hdrFmt );
        m_blurPipe    = PostUtil::createFullscreenPipeline( m_device, m_blurLay, "bloom_blur.frag.spv", hdrFmt );
        m_compPipe    = PostUtil::createFullscreenPipeline( m_device, m_compLay, "bloom_composite.frag.spv", hdrFmt );

        for ( uint32_t i = 0; i < k_frames; ++i )
        {
            m_extractSets[ i ] = PostUtil::allocSet( m_device, p_ctx.m_gpu->descPool(), m_samp1Layout );
            m_blurHSets[ i ]   = PostUtil::allocSet( m_device, p_ctx.m_gpu->descPool(), m_samp1Layout );
            m_blurVSets[ i ]   = PostUtil::allocSet( m_device, p_ctx.m_gpu->descPool(), m_samp1Layout );
            m_compSets[ i ]    = PostUtil::allocSet( m_device, p_ctx.m_gpu->descPool(), m_samp2Layout );
        }
    }

    void TPostBloom::onResize( const TPostContext& p_ctx )
    {
        ensurePipelines( p_ctx );

        m_half = { std::max( 1u, p_ctx.m_extent.width / 2 ), std::max( 1u, p_ctx.m_extent.height / 2 ) };

        const TVkImageDesc desc{ m_half.width,       m_half.height,   1,    1,    TImgFormat::RGBA16Float, TImgUsage::ColorAttachment | TImgUsage::Sampled,
                                 TTexFilter::Linear, TTexAddr::Clamp, true, false };
        m_brightA = TVkImage{};
        m_brightB = TVkImage{};
        m_brightA = TVkImage( p_ctx.m_gpu->device(), p_ctx.m_gpu->physDevice(), desc );
        m_brightB = TVkImage( p_ctx.m_gpu->device(), p_ctx.m_gpu->physDevice(), desc );
    }

    void TPostBloom::reloadShaders( const TPostContext& /*p_ctx*/ )
    {
        if ( m_device == VK_NULL_HANDLE || m_extractLay == VK_NULL_HANDLE ) return;
        const VkFormat hdrFmt = VK_FORMAT_R16G16B16A16_SFLOAT;
        if ( m_extractPipe != VK_NULL_HANDLE ) vkDestroyPipeline( m_device, m_extractPipe, nullptr );
        if ( m_blurPipe != VK_NULL_HANDLE ) vkDestroyPipeline( m_device, m_blurPipe, nullptr );
        if ( m_compPipe != VK_NULL_HANDLE ) vkDestroyPipeline( m_device, m_compPipe, nullptr );
        m_extractPipe = PostUtil::createFullscreenPipeline( m_device, m_extractLay, "bloom_extract.frag.spv", hdrFmt );
        m_blurPipe    = PostUtil::createFullscreenPipeline( m_device, m_blurLay, "bloom_blur.frag.spv", hdrFmt );
        m_compPipe    = PostUtil::createFullscreenPipeline( m_device, m_compLay, "bloom_composite.frag.spv", hdrFmt );
    }

    void TPostBloom::record( VkCommandBuffer p_cmd, TPostContext& p_ctx )
    {
        const uint32_t   fi   = p_ctx.m_frameIndex % k_frames;
        const VkExtent2D half = m_half;

        auto barrierToColor = [ & ]( TVkImage& img )
        {
            VkUtil::imageBarrier( p_cmd, img.handle(), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
                                  VK_ACCESS_2_NONE, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT );
        };
        auto barrierToSample = [ & ]( TVkImage& img )
        {
            VkUtil::imageBarrier( p_cmd, img.handle(), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                  VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                                  VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_SAMPLED_READ_BIT );
        };

        barrierToColor( m_brightA );
        PostUtil::writeCombinedImage( m_device, m_extractSets[ fi ], 0, *p_ctx.m_hdr );
        TExtractPC epc{ m_threshold, { 0, 0, 0 } };

        PostUtil::beginColorPass( p_cmd, m_brightA.view(), half );
        vkCmdBindPipeline( p_cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_extractPipe );
        vkCmdBindDescriptorSets( p_cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_extractLay, 0, 1, &m_extractSets[ fi ], 0, nullptr );
        vkCmdPushConstants( p_cmd, m_extractLay, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof( epc ), &epc );
        vkCmdDraw( p_cmd, 3, 1, 0, 0 );
        vkCmdEndRendering( p_cmd );
        barrierToSample( m_brightA );

        auto blurPass = [ & ]( TVkImage& src, TVkImage& dst, VkDescriptorSet set, float horizontal )
        {
            barrierToColor( dst );
            PostUtil::writeCombinedImage( m_device, set, 0, src );
            TBlurPC bpc{};
            bpc.m_texelSize  = { 1.0f / static_cast<float>( half.width ), 1.0f / static_cast<float>( half.height ) };
            bpc.m_horizontal = horizontal;
            PostUtil::beginColorPass( p_cmd, dst.view(), half );
            vkCmdBindPipeline( p_cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_blurPipe );
            vkCmdBindDescriptorSets( p_cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_blurLay, 0, 1, &set, 0, nullptr );
            vkCmdPushConstants( p_cmd, m_blurLay, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof( bpc ), &bpc );
            vkCmdDraw( p_cmd, 3, 1, 0, 0 );
            vkCmdEndRendering( p_cmd );
            barrierToSample( dst );
        };
        blurPass( m_brightA, m_brightB, m_blurHSets[ fi ], 1.0f );
        blurPass( m_brightB, m_brightA, m_blurVSets[ fi ], 0.0f );

        VkUtil::imageBarrier( p_cmd, p_ctx.m_hdrOther->handle(), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                              VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT, VK_ACCESS_2_NONE, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                              VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT );

        PostUtil::writeCombinedImage( m_device, m_compSets[ fi ], 0, *p_ctx.m_hdr );
        PostUtil::writeCombinedImage( m_device, m_compSets[ fi ], 1, m_brightA );
        TCompPC cpc{ m_strength, { 0, 0, 0 } };

        PostUtil::beginColorPass( p_cmd, p_ctx.m_hdrOther->view(), p_ctx.m_extent );
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

    void TPostBloom::destroy()
    {
        m_brightA = TVkImage{};
        m_brightB = TVkImage{};
        if ( m_device == VK_NULL_HANDLE ) return;
        vkDestroyPipeline( m_device, m_extractPipe, nullptr );
        vkDestroyPipeline( m_device, m_blurPipe, nullptr );
        vkDestroyPipeline( m_device, m_compPipe, nullptr );
        vkDestroyPipelineLayout( m_device, m_extractLay, nullptr );
        vkDestroyPipelineLayout( m_device, m_blurLay, nullptr );
        vkDestroyPipelineLayout( m_device, m_compLay, nullptr );
        vkDestroyDescriptorSetLayout( m_device, m_samp1Layout, nullptr );
        vkDestroyDescriptorSetLayout( m_device, m_samp2Layout, nullptr );
        m_extractPipe = m_blurPipe = m_compPipe = VK_NULL_HANDLE;
        m_device                                = VK_NULL_HANDLE;
    }
}  // namespace Tomos
