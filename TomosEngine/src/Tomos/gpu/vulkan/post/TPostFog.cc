#include "Tomos/gpu/vulkan/post/TPostFog.hh"

#include <glm/glm.hpp>

#include "Tomos/gpu/vulkan/TVkGpu.hh"
#include "Tomos/gpu/vulkan/TVkUtil.hh"
#include "Tomos/gpu/vulkan/post/TPostUtil.hh"

namespace Tomos
{
    namespace
    {
        struct TFogPC
        {
            glm::mat4 m_projInv;
            glm::vec4 m_fogColorDensity;
        };
    }  // namespace

    void TPostFog::onResize( const TPostContext& p_ctx )
    {
        m_device = p_ctx.m_gpu->device();

        if ( m_pipe == VK_NULL_HANDLE )
        {
            VkDescriptorSetLayoutBinding bindings[ 2 ]{};
            bindings[ 0 ] = { 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr };
            bindings[ 1 ] = { 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr };

            VkDescriptorSetLayoutCreateInfo li{};
            li.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
            li.bindingCount = 2;
            li.pBindings    = bindings;
            vkCreateDescriptorSetLayout( m_device, &li, nullptr, &m_layout );

            VkPushConstantRange pc{};
            pc.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
            pc.size       = sizeof( TFogPC );

            VkPipelineLayoutCreateInfo pl{};
            pl.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
            pl.setLayoutCount         = 1;
            pl.pSetLayouts            = &m_layout;
            pl.pushConstantRangeCount = 1;
            pl.pPushConstantRanges    = &pc;
            vkCreatePipelineLayout( m_device, &pl, nullptr, &m_pipeLay );

            m_pipe = PostUtil::createFullscreenPipeline( m_device, m_pipeLay, "fog.frag.spv", VK_FORMAT_R16G16B16A16_SFLOAT );
            for ( uint32_t i = 0; i < k_frames; ++i ) m_sets[ i ] = PostUtil::allocSet( m_device, p_ctx.m_gpu->descPool(), m_layout );
        }
    }

    void TPostFog::reloadShaders( const TPostContext& /*p_ctx*/ )
    {
        if ( m_device == VK_NULL_HANDLE || m_pipeLay == VK_NULL_HANDLE ) return;
        if ( m_pipe != VK_NULL_HANDLE ) vkDestroyPipeline( m_device, m_pipe, nullptr );
        m_pipe = PostUtil::createFullscreenPipeline( m_device, m_pipeLay, "fog.frag.spv", VK_FORMAT_R16G16B16A16_SFLOAT );
    }

    void TPostFog::record( VkCommandBuffer p_cmd, TPostContext& p_ctx )
    {
        const uint32_t fi = p_ctx.m_frameIndex % k_frames;

        VkUtil::imageBarrier( p_cmd, p_ctx.m_hdrOther->handle(), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                              VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT, VK_ACCESS_2_NONE, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                              VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT );

        PostUtil::writeCombinedImage( m_device, m_sets[ fi ], 0, *p_ctx.m_hdr );
        PostUtil::writeCombinedImage( m_device, m_sets[ fi ], 1, *p_ctx.m_depth );

        TFogPC pc{};
        pc.m_projInv         = p_ctx.m_projInv;
        pc.m_fogColorDensity = glm::vec4( m_color, m_density );

        PostUtil::beginColorPass( p_cmd, p_ctx.m_hdrOther->view(), p_ctx.m_extent );
        vkCmdBindPipeline( p_cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipe );
        vkCmdBindDescriptorSets( p_cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeLay, 0, 1, &m_sets[ fi ], 0, nullptr );
        vkCmdPushConstants( p_cmd, m_pipeLay, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof( pc ), &pc );
        vkCmdDraw( p_cmd, 3, 1, 0, 0 );
        vkCmdEndRendering( p_cmd );

        VkUtil::imageBarrier( p_cmd, p_ctx.m_hdrOther->handle(), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                              VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                              VK_ACCESS_2_SHADER_SAMPLED_READ_BIT );

        p_ctx.swapHdr();
    }

    void TPostFog::destroy()
    {
        if ( m_device == VK_NULL_HANDLE ) return;
        vkDestroyPipeline( m_device, m_pipe, nullptr );
        vkDestroyPipelineLayout( m_device, m_pipeLay, nullptr );
        vkDestroyDescriptorSetLayout( m_device, m_layout, nullptr );
        m_pipe    = VK_NULL_HANDLE;
        m_pipeLay = VK_NULL_HANDLE;
        m_layout  = VK_NULL_HANDLE;
        m_sets    = {};
        m_device  = VK_NULL_HANDLE;
    }
}  // namespace Tomos
