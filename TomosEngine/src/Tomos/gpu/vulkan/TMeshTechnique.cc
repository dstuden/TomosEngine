#include "Tomos/gpu/vulkan/TMeshTechnique.hh"

#include <stdexcept>

#include "Tomos/gpu/vulkan/TVkUtil.hh"

namespace Tomos
{
    namespace
    {
        void applyAlphaBlend( VkPipelineColorBlendAttachmentState& p_att )
        {
            p_att.blendEnable         = VK_TRUE;
            p_att.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
            p_att.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            p_att.colorBlendOp        = VK_BLEND_OP_ADD;
            p_att.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
            p_att.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            p_att.alphaBlendOp        = VK_BLEND_OP_ADD;
            p_att.colorWriteMask =
                    VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        }

        void createVariant( const TMeshTechniqueCreateCtx& p_ctx, const char* p_vertPath, const char* p_fragPath, bool p_skinned,
                            bool p_blend, bool p_depthWrite, const char* p_label, VkPipeline& p_out )
        {
            const VkDevice device = p_ctx.device;

            const VkShaderModule vertMod = VkUtil::loadSpv( device, p_vertPath );
            const VkShaderModule fragMod = VkUtil::loadSpv( device, p_fragPath );

            VkPipelineShaderStageCreateInfo stages[ 2 ]{};
            stages[ 0 ].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            stages[ 0 ].stage  = VK_SHADER_STAGE_VERTEX_BIT;
            stages[ 0 ].module = vertMod;
            stages[ 0 ].pName  = "main";
            stages[ 1 ].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            stages[ 1 ].stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
            stages[ 1 ].module = fragMod;
            stages[ 1 ].pName  = "main";

            const VkVertexInputBindingDescription staticBindings[] = {
                    { 0, sizeof( float ) * 3, VK_VERTEX_INPUT_RATE_VERTEX },
                    { 1, sizeof( float ) * 2, VK_VERTEX_INPUT_RATE_VERTEX },
                    { 2, sizeof( float ) * 3, VK_VERTEX_INPUT_RATE_VERTEX },
                    { 3, sizeof( float ) * 4, VK_VERTEX_INPUT_RATE_VERTEX },
            };
            const VkVertexInputAttributeDescription staticAttrs[] = {
                    { 0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0 },
                    { 1, 1, VK_FORMAT_R32G32_SFLOAT, 0 },
                    { 2, 2, VK_FORMAT_R32G32B32_SFLOAT, 0 },
                    { 3, 3, VK_FORMAT_R32G32B32A32_SFLOAT, 0 },
            };

            const VkVertexInputBindingDescription skinnedBindings[] = {
                    { 0, sizeof( float ) * 3, VK_VERTEX_INPUT_RATE_VERTEX },      { 1, sizeof( float ) * 2, VK_VERTEX_INPUT_RATE_VERTEX },
                    { 2, sizeof( float ) * 3, VK_VERTEX_INPUT_RATE_VERTEX },      { 3, sizeof( float ) * 4, VK_VERTEX_INPUT_RATE_VERTEX },
                    { 4, sizeof( uint32_t ) * 4, VK_VERTEX_INPUT_RATE_VERTEX }, { 5, sizeof( float ) * 4, VK_VERTEX_INPUT_RATE_VERTEX },
            };
            const VkVertexInputAttributeDescription skinnedAttrs[] = {
                    { 0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0 },    { 1, 1, VK_FORMAT_R32G32_SFLOAT, 0 },     { 2, 2, VK_FORMAT_R32G32B32_SFLOAT, 0 },
                    { 3, 3, VK_FORMAT_R32G32B32A32_SFLOAT, 0 }, { 4, 4, VK_FORMAT_R32G32B32A32_UINT, 0 }, { 5, 5, VK_FORMAT_R32G32B32A32_SFLOAT, 0 },
            };

            VkPipelineVertexInputStateCreateInfo vertexInput{};
            vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
            if ( p_skinned )
            {
                vertexInput.vertexBindingDescriptionCount   = 6;
                vertexInput.pVertexBindingDescriptions      = skinnedBindings;
                vertexInput.vertexAttributeDescriptionCount = 6;
                vertexInput.pVertexAttributeDescriptions    = skinnedAttrs;
            }
            else
            {
                vertexInput.vertexBindingDescriptionCount   = 4;
                vertexInput.pVertexBindingDescriptions      = staticBindings;
                vertexInput.vertexAttributeDescriptionCount = 4;
                vertexInput.pVertexAttributeDescriptions    = staticAttrs;
            }

            VkPipelineRasterizationStateCreateInfo raster{};
            raster.sType       = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
            raster.polygonMode = VK_POLYGON_MODE_FILL;
            // Cull mode is dynamic — set per draw from material->doubleSided().
            raster.cullMode  = VK_CULL_MODE_BACK_BIT;
            raster.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
            raster.lineWidth = 1.0f;

            VkPipelineDepthStencilStateCreateInfo depthStencil{};
            depthStencil.sType            = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
            depthStencil.depthTestEnable  = VK_TRUE;
            depthStencil.depthWriteEnable = p_depthWrite ? VK_TRUE : VK_FALSE;
            depthStencil.depthCompareOp   = VK_COMPARE_OP_LESS;

            VkPipelineColorBlendAttachmentState blendAtt{};
            blendAtt.colorWriteMask =
                    VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
            if ( p_blend ) applyAlphaBlend( blendAtt );

            VkPipelineColorBlendStateCreateInfo blend{};
            blend.sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
            blend.attachmentCount = 1;
            blend.pAttachments    = &blendAtt;

            const VkFormat                hdrFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
            VkPipelineRenderingCreateInfo renderingInfo{};
            renderingInfo.sType                   = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
            renderingInfo.colorAttachmentCount    = 1;
            renderingInfo.pColorAttachmentFormats = &hdrFormat;
            renderingInfo.depthAttachmentFormat   = VK_FORMAT_D32_SFLOAT;

            VkGraphicsPipelineCreateInfo pi{};
            pi.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
            pi.pNext               = &renderingInfo;
            pi.stageCount          = 2;
            pi.pStages             = stages;
            pi.pVertexInputState   = &vertexInput;
            pi.pInputAssemblyState = p_ctx.inputAsm;
            pi.pViewportState      = p_ctx.viewportState;
            pi.pRasterizationState = &raster;
            pi.pMultisampleState   = p_ctx.multisample;
            pi.pDepthStencilState  = &depthStencil;
            pi.pColorBlendState    = &blend;
            pi.pDynamicState       = p_ctx.dynamicState;
            pi.layout              = p_ctx.layout;

            if ( vkCreateGraphicsPipelines( device, VK_NULL_HANDLE, 1, &pi, nullptr, &p_out ) != VK_SUCCESS )
                throw std::runtime_error( std::string( "[TMeshTechnique] Failed to create " ) + p_label + " pipeline" );

            vkDestroyShaderModule( device, vertMod, nullptr );
            vkDestroyShaderModule( device, fragMod, nullptr );
        }
    }  // namespace

    void createMeshTechniquePipelines( const TMeshTechniqueCreateCtx& p_ctx, const TMeshTechniqueCreateDesc& p_desc,
                                       TMeshTechniquePipelines& p_out )
    {
        if ( p_desc.createOpaque )
        {
            createVariant( p_ctx, p_desc.staticVertSpv, p_desc.fragSpv, false, false, true, p_desc.label, p_out.opaque );
            createVariant( p_ctx, p_desc.skinnedVertSpv, p_desc.fragSpv, true, false, true, p_desc.label, p_out.skinnedOpaque );
        }
        if ( p_desc.createBlend )
        {
            createVariant( p_ctx, p_desc.staticVertSpv, p_desc.fragSpv, false, true, false, p_desc.label, p_out.blend );
            createVariant( p_ctx, p_desc.skinnedVertSpv, p_desc.fragSpv, true, true, false, p_desc.label, p_out.skinnedBlend );
        }
    }
}  // namespace Tomos
