#pragma once

#include <fstream>
#include <stdexcept>
#include <string>
#include <vulkan/vulkan.h>

#include "Tomos/gpu/vulkan/TVkUtil.hh"

namespace Tomos::PostUtil
{
    // No vertex buffers.
    inline VkPipeline createFullscreenPipeline( VkDevice p_device, VkPipelineLayout p_layout, const char* p_fragSpv, VkFormat p_colorFormat )
    {
        const VkShaderModule vertMod = VkUtil::loadSpv( p_device, VkUtil::resolveShaderPath( "fullscreen.vert.spv" ).c_str() );
        const VkShaderModule fragMod = VkUtil::loadSpv( p_device, VkUtil::resolveShaderPath( p_fragSpv ).c_str() );

        VkPipelineShaderStageCreateInfo stages[ 2 ]{};
        stages[ 0 ].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stages[ 0 ].stage  = VK_SHADER_STAGE_VERTEX_BIT;
        stages[ 0 ].module = vertMod;
        stages[ 0 ].pName  = "main";
        stages[ 1 ].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stages[ 1 ].stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
        stages[ 1 ].module = fragMod;
        stages[ 1 ].pName  = "main";

        VkPipelineVertexInputStateCreateInfo vertexInput{};
        vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

        VkPipelineInputAssemblyStateCreateInfo inputAsm{};
        inputAsm.sType    = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAsm.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

        VkPipelineViewportStateCreateInfo viewportState{};
        viewportState.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportState.viewportCount = 1;
        viewportState.scissorCount  = 1;

        VkPipelineRasterizationStateCreateInfo raster{};
        raster.sType       = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        raster.polygonMode = VK_POLYGON_MODE_FILL;
        raster.cullMode    = VK_CULL_MODE_NONE;
        raster.frontFace   = VK_FRONT_FACE_CLOCKWISE;
        raster.lineWidth   = 1.0f;

        VkPipelineMultisampleStateCreateInfo multisample{};
        multisample.sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        VkPipelineDepthStencilStateCreateInfo depth{};
        depth.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;

        VkPipelineColorBlendAttachmentState blendAtt{};
        blendAtt.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

        VkPipelineColorBlendStateCreateInfo blend{};
        blend.sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        blend.attachmentCount = 1;
        blend.pAttachments    = &blendAtt;

        const VkDynamicState             dynStates[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
        VkPipelineDynamicStateCreateInfo dynamicState{};
        dynamicState.sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamicState.dynamicStateCount = 2;
        dynamicState.pDynamicStates    = dynStates;

        VkPipelineRenderingCreateInfo renderingInfo{};
        renderingInfo.sType                   = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
        renderingInfo.colorAttachmentCount    = 1;
        renderingInfo.pColorAttachmentFormats = &p_colorFormat;

        VkGraphicsPipelineCreateInfo pi{};
        pi.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pi.pNext               = &renderingInfo;
        pi.stageCount          = 2;
        pi.pStages             = stages;
        pi.pVertexInputState   = &vertexInput;
        pi.pInputAssemblyState = &inputAsm;
        pi.pViewportState      = &viewportState;
        pi.pRasterizationState = &raster;
        pi.pMultisampleState   = &multisample;
        pi.pDepthStencilState  = &depth;
        pi.pColorBlendState    = &blend;
        pi.pDynamicState       = &dynamicState;
        pi.layout              = p_layout;

        VkPipeline pipeline = VK_NULL_HANDLE;
        if ( vkCreateGraphicsPipelines( p_device, VK_NULL_HANDLE, 1, &pi, nullptr, &pipeline ) != VK_SUCCESS )
            throw std::runtime_error( std::string( "[PostUtil] Failed to create pipeline for " ) + p_fragSpv );

        vkDestroyShaderModule( p_device, vertMod, nullptr );
        vkDestroyShaderModule( p_device, fragMod, nullptr );
        return pipeline;
    }

    inline void beginColorPass( VkCommandBuffer p_cmd, VkImageView p_view, VkExtent2D p_extent, VkAttachmentLoadOp p_load = VK_ATTACHMENT_LOAD_OP_DONT_CARE )
    {
        VkRenderingAttachmentInfo colorAtt{};
        colorAtt.sType       = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        colorAtt.imageView   = p_view;
        colorAtt.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        colorAtt.loadOp      = p_load;
        colorAtt.storeOp     = VK_ATTACHMENT_STORE_OP_STORE;
        colorAtt.clearValue  = { { { 0.0f, 0.0f, 0.0f, 1.0f } } };

        VkRenderingInfo rendering{};
        rendering.sType                = VK_STRUCTURE_TYPE_RENDERING_INFO;
        rendering.renderArea.extent    = p_extent;
        rendering.layerCount           = 1;
        rendering.colorAttachmentCount = 1;
        rendering.pColorAttachments    = &colorAtt;

        vkCmdBeginRendering( p_cmd, &rendering );

        VkViewport viewport{};
        viewport.width    = static_cast<float>( p_extent.width );
        viewport.height   = static_cast<float>( p_extent.height );
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport( p_cmd, 0, 1, &viewport );

        VkRect2D scissor{};
        scissor.extent = p_extent;
        vkCmdSetScissor( p_cmd, 0, 1, &scissor );
    }

    inline VkDescriptorSet allocSet( VkDevice p_device, VkDescriptorPool p_pool, VkDescriptorSetLayout p_layout )
    {
        VkDescriptorSetAllocateInfo ai{};
        ai.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        ai.descriptorPool     = p_pool;
        ai.descriptorSetCount = 1;
        ai.pSetLayouts        = &p_layout;

        VkDescriptorSet set = VK_NULL_HANDLE;
        if ( vkAllocateDescriptorSets( p_device, &ai, &set ) != VK_SUCCESS ) throw std::runtime_error( "[PostUtil] Failed to allocate descriptor set" );
        return set;
    }

    inline void writeCombinedImage( VkDevice p_device, VkDescriptorSet p_set, uint32_t p_binding, const TVkImage& p_image )
    {
        VkDescriptorImageInfo img{ p_image.sampler(), p_image.view(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
        VkWriteDescriptorSet  write{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,    nullptr, p_set,   p_binding, 0, 1,
                                     VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &img,    nullptr, nullptr };
        vkUpdateDescriptorSets( p_device, 1, &write, 0, nullptr );
    }
}  // namespace Tomos::PostUtil
