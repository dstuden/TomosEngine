#include "Tomos/gpu/vulkan/post/TPostSSAO.hh"

#include <glm/glm.hpp>

#include "Tomos/gpu/vulkan/TVkGpu.hh"
#include "Tomos/gpu/vulkan/TVkUtil.hh"
#include "Tomos/gpu/vulkan/post/TPostUtil.hh"

namespace Tomos
{
    namespace
    {
        struct TSsaoPC
        {
            glm::mat4 m_projInv;
            glm::vec4 m_params;
        };
    }  // namespace

    void TPostSSAO::onResize( const TPostContext& p_ctx )
    {
        m_device = p_ctx.m_gpu->device();

        if ( m_aoPipe == VK_NULL_HANDLE )
        {
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

            VkPushConstantRange pc{};
            pc.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
            pc.size       = sizeof( TSsaoPC );

            VkPipelineLayoutCreateInfo plAo{};
            plAo.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
            plAo.setLayoutCount         = 1;
            plAo.pSetLayouts            = &m_samp1Layout;
            plAo.pushConstantRangeCount = 1;
            plAo.pPushConstantRanges    = &pc;
            vkCreatePipelineLayout( m_device, &plAo, nullptr, &m_aoLay );

            VkPipelineLayoutCreateInfo plComp{};
            plComp.sType          = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
            plComp.setLayoutCount = 1;
            plComp.pSetLayouts    = &m_samp2Layout;
            vkCreatePipelineLayout( m_device, &plComp, nullptr, &m_compLay );

            m_aoPipe   = PostUtil::createFullscreenPipeline( m_device, m_aoLay, "ssao.frag.spv", VK_FORMAT_R16G16B16A16_SFLOAT );
            m_compPipe = PostUtil::createFullscreenPipeline( m_device, m_compLay, "ssao_compose.frag.spv", VK_FORMAT_R16G16B16A16_SFLOAT );

            for ( uint32_t i = 0; i < g_kFrames; ++i )
            {
                m_aoSets[ i ]   = PostUtil::allocSet( m_device, p_ctx.m_gpu->descPool(), m_samp1Layout );
                m_compSets[ i ] = PostUtil::allocSet( m_device, p_ctx.m_gpu->descPool(), m_samp2Layout );
            }
        }

        const TVkImageDesc desc{
                p_ctx.m_extent.width, p_ctx.m_extent.height, 1,    1,    TImgFormat::RGBA16Float, TImgUsage::ColorAttachment | TImgUsage::Sampled,
                TTexFilter::Linear,   TTexAddr::Clamp,       true, false };
        m_ao = TVkImage{};
        m_ao = TVkImage( p_ctx.m_gpu->device(), p_ctx.m_gpu->physDevice(), desc );
    }

    void TPostSSAO::reloadShaders( const TPostContext& /*p_ctx*/ )
    {
        if ( m_device == VK_NULL_HANDLE || m_aoLay == VK_NULL_HANDLE ) return;
        if ( m_aoPipe != VK_NULL_HANDLE ) vkDestroyPipeline( m_device, m_aoPipe, nullptr );
        if ( m_compPipe != VK_NULL_HANDLE ) vkDestroyPipeline( m_device, m_compPipe, nullptr );
        m_aoPipe   = PostUtil::createFullscreenPipeline( m_device, m_aoLay, "ssao.frag.spv", VK_FORMAT_R16G16B16A16_SFLOAT );
        m_compPipe = PostUtil::createFullscreenPipeline( m_device, m_compLay, "ssao_compose.frag.spv", VK_FORMAT_R16G16B16A16_SFLOAT );
    }

    void TPostSSAO::record( VkCommandBuffer p_cmd, TPostContext& p_ctx )
    {
        const uint32_t fi = p_ctx.m_frameIndex % g_kFrames;

        VkUtil::imageBarrier( p_cmd, m_ao.handle(), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
                              VK_ACCESS_2_NONE, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT );

        PostUtil::writeCombinedImage( m_device, m_aoSets[ fi ], 0, *p_ctx.m_depth );

        TSsaoPC pc{};
        pc.m_projInv = p_ctx.m_projInv;
        pc.m_params  = glm::vec4( m_radius, m_bias, m_intensity, 0.0f );  // radius is world-space

        PostUtil::beginColorPass( p_cmd, m_ao.view(), p_ctx.m_extent );
        vkCmdBindPipeline( p_cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_aoPipe );
        vkCmdBindDescriptorSets( p_cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_aoLay, 0, 1, &m_aoSets[ fi ], 0, nullptr );
        vkCmdPushConstants( p_cmd, m_aoLay, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof( pc ), &pc );
        vkCmdDraw( p_cmd, 3, 1, 0, 0 );
        vkCmdEndRendering( p_cmd );

        VkUtil::imageBarrier( p_cmd, m_ao.handle(), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                              VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                              VK_ACCESS_2_SHADER_SAMPLED_READ_BIT );

        VkUtil::imageBarrier( p_cmd, p_ctx.m_hdrOther->handle(), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                              VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT, VK_ACCESS_2_NONE, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                              VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT );

        PostUtil::writeCombinedImage( m_device, m_compSets[ fi ], 0, *p_ctx.m_hdr );
        PostUtil::writeCombinedImage( m_device, m_compSets[ fi ], 1, m_ao );

        PostUtil::beginColorPass( p_cmd, p_ctx.m_hdrOther->view(), p_ctx.m_extent );
        vkCmdBindPipeline( p_cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_compPipe );
        vkCmdBindDescriptorSets( p_cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_compLay, 0, 1, &m_compSets[ fi ], 0, nullptr );
        vkCmdDraw( p_cmd, 3, 1, 0, 0 );
        vkCmdEndRendering( p_cmd );

        VkUtil::imageBarrier( p_cmd, p_ctx.m_hdrOther->handle(), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                              VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                              VK_ACCESS_2_SHADER_SAMPLED_READ_BIT );

        p_ctx.swapHdr();
    }

    void TPostSSAO::destroy()
    {
        m_ao = TVkImage{};
        if ( m_device == VK_NULL_HANDLE ) return;
        vkDestroyPipeline( m_device, m_aoPipe, nullptr );
        vkDestroyPipeline( m_device, m_compPipe, nullptr );
        vkDestroyPipelineLayout( m_device, m_aoLay, nullptr );
        vkDestroyPipelineLayout( m_device, m_compLay, nullptr );
        vkDestroyDescriptorSetLayout( m_device, m_samp1Layout, nullptr );
        vkDestroyDescriptorSetLayout( m_device, m_samp2Layout, nullptr );
        m_aoPipe = m_compPipe = VK_NULL_HANDLE;
        m_device              = VK_NULL_HANDLE;
    }
}  // namespace Tomos
