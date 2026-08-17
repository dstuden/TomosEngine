#include "Tomos/gpu/vulkan/post/TPostTonemap.hh"

#include "Tomos/gpu/vulkan/TVkGpu.hh"
#include "Tomos/gpu/vulkan/TVkUtil.hh"
#include "Tomos/gpu/vulkan/post/TPostUtil.hh"

namespace Tomos
{
    void TPostTonemap::onResize( const TPostContext& p_ctx )
    {
        m_device = p_ctx.m_gpu->device();

        if ( m_pipe == VK_NULL_HANDLE )
        {
            VkDescriptorSetLayoutBinding binding{};
            binding.binding         = 0;
            binding.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            binding.descriptorCount = 1;
            binding.stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT;

            VkDescriptorSetLayoutCreateInfo li{};
            li.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
            li.bindingCount = 1;
            li.pBindings    = &binding;
            vkCreateDescriptorSetLayout( m_device, &li, nullptr, &m_layout );

            VkPipelineLayoutCreateInfo pl{};
            pl.sType          = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
            pl.setLayoutCount = 1;
            pl.pSetLayouts    = &m_layout;
            vkCreatePipelineLayout( m_device, &pl, nullptr, &m_pipeLay );

            m_pipe = PostUtil::createFullscreenPipeline( m_device, m_pipeLay, "tonemap.frag.spv", p_ctx.m_outputFormat );
            for ( uint32_t i = 0; i < g_kFrames; ++i ) m_sets[ i ] = PostUtil::allocSet( m_device, p_ctx.m_gpu->descPool(), m_layout );
        }
        else
        {
            vkDestroyPipeline( m_device, m_pipe, nullptr );
            m_pipe = PostUtil::createFullscreenPipeline( m_device, m_pipeLay, "tonemap.frag.spv", p_ctx.m_outputFormat );
        }
    }

    void TPostTonemap::reloadShaders( const TPostContext& p_ctx )
    {
        if ( m_device == VK_NULL_HANDLE || m_pipeLay == VK_NULL_HANDLE ) return;
        if ( m_pipe != VK_NULL_HANDLE ) vkDestroyPipeline( m_device, m_pipe, nullptr );
        m_pipe = PostUtil::createFullscreenPipeline( m_device, m_pipeLay, "tonemap.frag.spv", p_ctx.m_outputFormat );
    }

    void TPostTonemap::record( VkCommandBuffer p_cmd, TPostContext& p_ctx )
    {
        const uint32_t fi = p_ctx.m_frameIndex % g_kFrames;

        VkUtil::imageBarrier( p_cmd, p_ctx.m_outputImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                              VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT, VK_ACCESS_2_NONE, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                              VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT );

        PostUtil::writeCombinedImage( m_device, m_sets[ fi ], 0, *p_ctx.m_hdr );

        PostUtil::beginColorPass( p_cmd, p_ctx.m_outputView, p_ctx.m_extent, VK_ATTACHMENT_LOAD_OP_DONT_CARE );
        vkCmdBindPipeline( p_cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipe );
        vkCmdBindDescriptorSets( p_cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeLay, 0, 1, &m_sets[ fi ], 0, nullptr );
        vkCmdDraw( p_cmd, 3, 1, 0, 0 );
        vkCmdEndRendering( p_cmd );
    }

    void TPostTonemap::destroy()
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
