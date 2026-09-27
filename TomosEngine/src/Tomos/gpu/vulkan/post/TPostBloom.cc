#include "Tomos/gpu/vulkan/post/TPostBloom.hh"

#include <algorithm>
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
        struct TKawasePC
        {
            glm::vec2 m_halfPixel;
            glm::vec2 m_pad;
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

        makeLay( m_samp1Layout, sizeof( TExtractPC ), &m_extractLay );
        makeLay( m_samp1Layout, sizeof( TKawasePC ), &m_downLay );
        makeLay( m_samp1Layout, sizeof( TKawasePC ), &m_upLay );
        makeLay( m_samp2Layout, sizeof( TCompPC ), &m_compLay );

        m_extractPipe = PostUtil::createFullscreenPipeline( m_device, m_extractLay, "bloom_extract.frag.spv", hdrFmt );
        m_downPipe    = PostUtil::createFullscreenPipeline( m_device, m_downLay, "bloom_downsample.frag.spv", hdrFmt );
        m_upPipe      = PostUtil::createFullscreenPipeline( m_device, m_upLay, "bloom_upsample.frag.spv", hdrFmt );
        m_compPipe    = PostUtil::createFullscreenPipeline( m_device, m_compLay, "bloom_composite.frag.spv", hdrFmt );

        for ( uint32_t i = 0; i < g_kFrames; ++i )
        {
            m_extractSets[ i ] = PostUtil::allocSet( m_device, p_ctx.m_gpu->descPool(), m_samp1Layout );
            m_compSets[ i ]    = PostUtil::allocSet( m_device, p_ctx.m_gpu->descPool(), m_samp2Layout );
            for ( uint32_t l = 0; l < g_kMaxLevels; ++l )
            {
                m_downSets[ i ][ l ] = PostUtil::allocSet( m_device, p_ctx.m_gpu->descPool(), m_samp1Layout );
                m_upSets[ i ][ l ]   = PostUtil::allocSet( m_device, p_ctx.m_gpu->descPool(), m_samp1Layout );
            }
        }
    }

    void TPostBloom::onResize( const TPostContext& p_ctx )
    {
        ensurePipelines( p_ctx );

        const uint32_t levels = static_cast<uint32_t>( std::clamp( m_iterations, 2, static_cast<int>( g_kMaxLevels ) ) );
        m_levelCount          = levels;

        uint32_t w = std::max( 1u, p_ctx.m_extent.width / 2 );
        uint32_t h = std::max( 1u, p_ctx.m_extent.height / 2 );

        for ( uint32_t i = 0; i < g_kMaxLevels; ++i ) m_mips[ i ] = TVkImage{};

        for ( uint32_t i = 0; i < levels; ++i )
        {
            const TVkImageDesc desc{ w, h, 1, 1, TImgFormat::RGBA16Float, TImgUsage::ColorAttachment | TImgUsage::Sampled,
                                     TTexFilter::Linear, TTexAddr::Clamp, true, false };
            m_mips[ i ] = TVkImage( p_ctx.m_gpu->device(), p_ctx.m_gpu->physDevice(), desc );
            w           = std::max( 1u, w / 2 );
            h           = std::max( 1u, h / 2 );
        }
    }

    void TPostBloom::record( VkCommandBuffer p_cmd, TPostContext& p_ctx )
    {
        const uint32_t levels = static_cast<uint32_t>( std::clamp( m_iterations, 2, static_cast<int>( g_kMaxLevels ) ) );
        if ( levels != m_levelCount || !m_mips[ 0 ].valid() ) onResize( p_ctx );

        const uint32_t fi = p_ctx.m_frameIndex % g_kFrames;

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

        // Extract bright → mip[0] (half-res).
        {
            const VkExtent2D ext{ m_mips[ 0 ].width(), m_mips[ 0 ].height() };
            barrierToColor( m_mips[ 0 ] );
            PostUtil::writeCombinedImage( m_device, m_extractSets[ fi ], 0, *p_ctx.m_hdr );
            TExtractPC epc{ m_threshold, { 0, 0, 0 } };

            PostUtil::beginColorPass( p_cmd, m_mips[ 0 ].view(), ext );
            vkCmdBindPipeline( p_cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_extractPipe );
            vkCmdBindDescriptorSets( p_cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_extractLay, 0, 1, &m_extractSets[ fi ], 0, nullptr );
            vkCmdPushConstants( p_cmd, m_extractLay, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof( epc ), &epc );
            vkCmdDraw( p_cmd, 3, 1, 0, 0 );
            vkCmdEndRendering( p_cmd );
            barrierToSample( m_mips[ 0 ] );
        }

        auto kawasePass = [ & ]( TVkImage& p_src, TVkImage& p_dst, VkPipeline p_pipe, VkPipelineLayout p_lay, VkDescriptorSet p_set )
        {
            const VkExtent2D ext{ p_dst.width(), p_dst.height() };
            barrierToColor( p_dst );
            PostUtil::writeCombinedImage( m_device, p_set, 0, p_src );
            TKawasePC kpc{};
            kpc.m_halfPixel = { 0.5f / static_cast<float>( p_src.width() ), 0.5f / static_cast<float>( p_src.height() ) };

            PostUtil::beginColorPass( p_cmd, p_dst.view(), ext );
            vkCmdBindPipeline( p_cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, p_pipe );
            vkCmdBindDescriptorSets( p_cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, p_lay, 0, 1, &p_set, 0, nullptr );
            vkCmdPushConstants( p_cmd, p_lay, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof( kpc ), &kpc );
            vkCmdDraw( p_cmd, 3, 1, 0, 0 );
            vkCmdEndRendering( p_cmd );
            barrierToSample( p_dst );
        };

        // Downsample pyramid.
        for ( uint32_t i = 0; i + 1 < m_levelCount; ++i )
            kawasePass( m_mips[ i ], m_mips[ i + 1 ], m_downPipe, m_downLay, m_downSets[ fi ][ i ] );

        // Upsample back to mip[0].
        for ( uint32_t i = m_levelCount - 1; i > 0; --i )
            kawasePass( m_mips[ i ], m_mips[ i - 1 ], m_upPipe, m_upLay, m_upSets[ fi ][ i ] );

        // Composite into full HDR.
        VkUtil::imageBarrier( p_cmd, p_ctx.m_hdrOther->handle(), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                              VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT, VK_ACCESS_2_NONE, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                              VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT );

        PostUtil::writeCombinedImage( m_device, m_compSets[ fi ], 0, *p_ctx.m_hdr );
        PostUtil::writeCombinedImage( m_device, m_compSets[ fi ], 1, m_mips[ 0 ] );
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
        for ( auto& m : m_mips ) m = TVkImage{};
        m_levelCount = 0;
        if ( m_device == VK_NULL_HANDLE ) return;
        vkDestroyPipeline( m_device, m_extractPipe, nullptr );
        vkDestroyPipeline( m_device, m_downPipe, nullptr );
        vkDestroyPipeline( m_device, m_upPipe, nullptr );
        vkDestroyPipeline( m_device, m_compPipe, nullptr );
        vkDestroyPipelineLayout( m_device, m_extractLay, nullptr );
        vkDestroyPipelineLayout( m_device, m_downLay, nullptr );
        vkDestroyPipelineLayout( m_device, m_upLay, nullptr );
        vkDestroyPipelineLayout( m_device, m_compLay, nullptr );
        vkDestroyDescriptorSetLayout( m_device, m_samp1Layout, nullptr );
        vkDestroyDescriptorSetLayout( m_device, m_samp2Layout, nullptr );
        m_extractPipe = m_downPipe = m_upPipe = m_compPipe = VK_NULL_HANDLE;
        m_device                                           = VK_NULL_HANDLE;
    }
}  // namespace Tomos
