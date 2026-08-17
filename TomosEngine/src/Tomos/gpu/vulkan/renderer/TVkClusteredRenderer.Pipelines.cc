#include "Tomos/gpu/vulkan/renderer/TVkClusteredRenderer.hh"

#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "Tomos/gpu/vulkan/TMeshTechnique.hh"
#include "Tomos/gpu/vulkan/TVkGpu.hh"
#include "Tomos/gpu/vulkan/TVkUtil.hh"

namespace Tomos
{
    void TVkClusteredRenderer::destroyGraphicsPipelines()
    {
        const VkDevice device  = m_gpu.device();
        auto           destroy = [ & ]( VkPipeline& p_p )
        {
            if ( p_p != VK_NULL_HANDLE )
            {
                vkDestroyPipeline( device, p_p, nullptr );
                p_p = VK_NULL_HANDLE;
            }
        };
        destroy( m_shadowPipeline );
        destroy( m_skinnedShadowPipeline );
        destroy( m_cullPipeline );
        for ( auto& tech : m_meshTechniques ) tech.destroyAll( device );
        destroy( m_spritePipeline );
        destroy( m_particleSimPipeline );
        destroy( m_particlePipeline );
    }

    void TVkClusteredRenderer::reloadShaders()
    {
        destroyGraphicsPipelines();
        createGraphicsPipelines();
        m_post.reloadShaders( makePostContext() );
    }

    void TVkClusteredRenderer::createPipelines()
    {
        createPipelineLayouts();
        createGraphicsPipelines();
    }

    void TVkClusteredRenderer::createPipelineLayouts()
    {
        const VkDevice device = m_gpu.device();

        {
            const VkDescriptorSetLayout setLayouts[] = { m_sceneLayout, m_gpu.layouts().m_material };
            VkPipelineLayoutCreateInfo  li{};
            li.sType          = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
            li.setLayoutCount = 2;
            li.pSetLayouts    = setLayouts;
            if ( vkCreatePipelineLayout( device, &li, nullptr, &m_forwardPipeLayout ) != VK_SUCCESS )
                throw std::runtime_error( "[TVkClusteredRenderer] Failed to create forward pipeline layout" );
        }
        {
            VkPushConstantRange        push{ VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof( glm::mat4 ) };
            VkPipelineLayoutCreateInfo li{};
            li.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
            li.setLayoutCount         = 1;
            li.pSetLayouts            = &m_sceneLayout;
            li.pushConstantRangeCount = 1;
            li.pPushConstantRanges    = &push;
            if ( vkCreatePipelineLayout( device, &li, nullptr, &m_shadowPipeLayout ) != VK_SUCCESS )
                throw std::runtime_error( "[TVkClusteredRenderer] Failed to create shadow pipeline layout" );
        }
        {
            VkPipelineLayoutCreateInfo li{};
            li.sType          = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
            li.setLayoutCount = 1;
            li.pSetLayouts    = &m_cullLayout;
            if ( vkCreatePipelineLayout( device, &li, nullptr, &m_cullPipeLayout ) != VK_SUCCESS )
                throw std::runtime_error( "[TVkClusteredRenderer] Failed to create cull pipeline layout" );
        }
        {
            const VkDescriptorSetLayout setLayouts[] = { m_spriteSetLayout, m_spriteTexLayout };
            VkPipelineLayoutCreateInfo  li{};
            li.sType          = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
            li.setLayoutCount = 2;
            li.pSetLayouts    = setLayouts;
            if ( vkCreatePipelineLayout( device, &li, nullptr, &m_spritePipeLayout ) != VK_SUCCESS )
                throw std::runtime_error( "[TVkClusteredRenderer] Failed to create sprite pipeline layout" );
        }
        {
            VkPushConstantRange        push{ VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof( uint32_t ) };
            VkPipelineLayoutCreateInfo li{};
            li.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
            li.setLayoutCount         = 1;
            li.pSetLayouts            = &m_particleSimLayout;
            li.pushConstantRangeCount = 1;
            li.pPushConstantRanges    = &push;
            if ( vkCreatePipelineLayout( device, &li, nullptr, &m_particleSimPipeLayout ) != VK_SUCCESS )
                throw std::runtime_error( "[TVkClusteredRenderer] Failed to create particle sim pipeline layout" );
        }
        {
            const VkDescriptorSetLayout setLayouts[] = { m_particleDrawLayout, m_spriteTexLayout };
            VkPushConstantRange         push{ VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof( uint32_t ) };
            VkPipelineLayoutCreateInfo  li{};
            li.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
            li.setLayoutCount         = 2;
            li.pSetLayouts            = setLayouts;
            li.pushConstantRangeCount = 1;
            li.pPushConstantRanges    = &push;
            if ( vkCreatePipelineLayout( device, &li, nullptr, &m_particleDrawPipeLayout ) != VK_SUCCESS )
                throw std::runtime_error( "[TVkClusteredRenderer] Failed to create particle draw pipeline layout" );
        }
    }

    void TVkClusteredRenderer::createGraphicsPipelines()
    {
        const VkDevice device = m_gpu.device();

        {
            const VkShaderModule compMod = VkUtil::loadSpv( device, VkUtil::resolveShaderPath( "cluster_cull.comp.spv" ).c_str() );

            VkComputePipelineCreateInfo ci{};
            ci.sType        = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
            ci.stage.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            ci.stage.stage  = VK_SHADER_STAGE_COMPUTE_BIT;
            ci.stage.module = compMod;
            ci.stage.pName  = "main";
            ci.layout       = m_cullPipeLayout;

            if ( vkCreateComputePipelines( device, VK_NULL_HANDLE, 1, &ci, nullptr, &m_cullPipeline ) != VK_SUCCESS )
                throw std::runtime_error( "[TVkClusteredRenderer] Failed to create cull pipeline" );

            vkDestroyShaderModule( device, compMod, nullptr );
        }

        {
            const VkShaderModule compMod = VkUtil::loadSpv( device, VkUtil::resolveShaderPath( "particle_sim.comp.spv" ).c_str() );

            VkComputePipelineCreateInfo ci{};
            ci.sType        = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
            ci.stage.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            ci.stage.stage  = VK_SHADER_STAGE_COMPUTE_BIT;
            ci.stage.module = compMod;
            ci.stage.pName  = "main";
            ci.layout       = m_particleSimPipeLayout;

            if ( vkCreateComputePipelines( device, VK_NULL_HANDLE, 1, &ci, nullptr, &m_particleSimPipeline ) != VK_SUCCESS )
                throw std::runtime_error( "[TVkClusteredRenderer] Failed to create particle sim pipeline" );

            vkDestroyShaderModule( device, compMod, nullptr );
        }

        VkPipelineInputAssemblyStateCreateInfo inputAsm{};
        inputAsm.sType    = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAsm.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

        VkPipelineViewportStateCreateInfo viewportState{};
        viewportState.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportState.viewportCount = 1;
        viewportState.scissorCount  = 1;

        VkPipelineMultisampleStateCreateInfo multisample{};
        multisample.sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        VkPipelineDepthStencilStateCreateInfo depthStencil{};
        depthStencil.sType            = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depthStencil.depthTestEnable  = VK_TRUE;
        depthStencil.depthWriteEnable = VK_TRUE;
        depthStencil.depthCompareOp   = VK_COMPARE_OP_LESS;

        const VkDynamicState             dynStates[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR, VK_DYNAMIC_STATE_CULL_MODE };
        VkPipelineDynamicStateCreateInfo dynamicState{};
        dynamicState.sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamicState.dynamicStateCount = 3;
        dynamicState.pDynamicStates    = dynStates;

        TMeshTechniqueCreateCtx techCtx{};
        techCtx.m_device        = device;
        techCtx.m_layout        = m_forwardPipeLayout;
        techCtx.m_inputAsm      = &inputAsm;
        techCtx.m_viewportState = &viewportState;
        techCtx.m_multisample   = &multisample;
        techCtx.m_dynamicState  = &dynamicState;

        {
            const std::string        staticVert  = VkUtil::resolveShaderPath( "forward.vert.spv" );
            const std::string        skinnedVert = VkUtil::resolveShaderPath( "skinned.vert.spv" );
            const std::string        frag        = VkUtil::resolveShaderPath( "forward.frag.spv" );
            TMeshTechniqueCreateDesc desc{};
            desc.m_staticVertSpv  = staticVert.c_str();
            desc.m_skinnedVertSpv = skinnedVert.c_str();
            desc.m_fragSpv        = frag.c_str();
            desc.m_label          = "forward";
            desc.m_createOpaque   = true;
            desc.m_createBlend    = true;
            createMeshTechniquePipelines( techCtx, desc, m_meshTechniques[ static_cast<size_t>( TMeshTechniqueId::Forward ) ] );
        }

        {
            const std::string        staticVert  = VkUtil::resolveShaderPath( "water.vert.spv" );
            const std::string        skinnedVert = VkUtil::resolveShaderPath( "skinned.vert.spv" );
            const std::string        frag        = VkUtil::resolveShaderPath( "water.frag.spv" );
            TMeshTechniqueCreateDesc desc{};
            desc.m_staticVertSpv  = staticVert.c_str();
            desc.m_skinnedVertSpv = skinnedVert.c_str();
            desc.m_fragSpv        = frag.c_str();
            desc.m_label          = "water";
            desc.m_createOpaque   = false;
            desc.m_createBlend    = true;
            createMeshTechniquePipelines( techCtx, desc, m_meshTechniques[ static_cast<size_t>( TMeshTechniqueId::Water ) ] );
        }

        {
            const VkShaderModule vertMod = VkUtil::loadSpv( device, VkUtil::resolveShaderPath( "sprite.vert.spv" ).c_str() );
            const VkShaderModule fragMod = VkUtil::loadSpv( device, VkUtil::resolveShaderPath( "sprite.frag.spv" ).c_str() );

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

            VkPipelineRasterizationStateCreateInfo raster{};
            raster.sType       = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
            raster.polygonMode = VK_POLYGON_MODE_FILL;
            raster.cullMode    = VK_CULL_MODE_NONE;
            raster.frontFace   = VK_FRONT_FACE_COUNTER_CLOCKWISE;
            raster.lineWidth   = 1.0f;

            VkPipelineDepthStencilStateCreateInfo spriteDepth{};
            spriteDepth.sType            = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
            spriteDepth.depthTestEnable  = VK_TRUE;
            spriteDepth.depthWriteEnable = VK_FALSE;
            spriteDepth.depthCompareOp   = VK_COMPARE_OP_LESS;

            VkPipelineColorBlendAttachmentState blendAtt{};
            blendAtt.blendEnable         = VK_TRUE;
            blendAtt.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
            blendAtt.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            blendAtt.colorBlendOp        = VK_BLEND_OP_ADD;
            blendAtt.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
            blendAtt.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            blendAtt.alphaBlendOp        = VK_BLEND_OP_ADD;
            blendAtt.colorWriteMask      = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

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
            pi.pInputAssemblyState = &inputAsm;
            pi.pViewportState      = &viewportState;
            pi.pRasterizationState = &raster;
            pi.pMultisampleState   = &multisample;
            pi.pDepthStencilState  = &spriteDepth;
            pi.pColorBlendState    = &blend;
            pi.pDynamicState       = &dynamicState;
            pi.layout              = m_spritePipeLayout;

            if ( vkCreateGraphicsPipelines( device, VK_NULL_HANDLE, 1, &pi, nullptr, &m_spritePipeline ) != VK_SUCCESS )
                throw std::runtime_error( "[TVkClusteredRenderer] Failed to create sprite pipeline" );

            vkDestroyShaderModule( device, vertMod, nullptr );
            vkDestroyShaderModule( device, fragMod, nullptr );
        }

        {
            const VkShaderModule vertMod = VkUtil::loadSpv( device, VkUtil::resolveShaderPath( "particle.vert.spv" ).c_str() );
            const VkShaderModule fragMod = VkUtil::loadSpv( device, VkUtil::resolveShaderPath( "particle.frag.spv" ).c_str() );

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

            VkPipelineRasterizationStateCreateInfo raster{};
            raster.sType       = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
            raster.polygonMode = VK_POLYGON_MODE_FILL;
            raster.cullMode    = VK_CULL_MODE_NONE;
            raster.frontFace   = VK_FRONT_FACE_COUNTER_CLOCKWISE;
            raster.lineWidth   = 1.0f;

            VkPipelineDepthStencilStateCreateInfo particleDepth{};
            particleDepth.sType            = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
            particleDepth.depthTestEnable  = VK_TRUE;
            particleDepth.depthWriteEnable = VK_FALSE;
            particleDepth.depthCompareOp   = VK_COMPARE_OP_LESS;

            VkPipelineColorBlendAttachmentState blendAtt{};
            blendAtt.blendEnable         = VK_TRUE;
            blendAtt.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
            blendAtt.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
            blendAtt.colorBlendOp        = VK_BLEND_OP_ADD;
            blendAtt.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
            blendAtt.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
            blendAtt.alphaBlendOp        = VK_BLEND_OP_ADD;
            blendAtt.colorWriteMask      = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

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
            pi.pInputAssemblyState = &inputAsm;
            pi.pViewportState      = &viewportState;
            pi.pRasterizationState = &raster;
            pi.pMultisampleState   = &multisample;
            pi.pDepthStencilState  = &particleDepth;
            pi.pColorBlendState    = &blend;
            pi.pDynamicState       = &dynamicState;
            pi.layout              = m_particleDrawPipeLayout;

            if ( vkCreateGraphicsPipelines( device, VK_NULL_HANDLE, 1, &pi, nullptr, &m_particlePipeline ) != VK_SUCCESS )
                throw std::runtime_error( "[TVkClusteredRenderer] Failed to create particle pipeline" );

            vkDestroyShaderModule( device, vertMod, nullptr );
            vkDestroyShaderModule( device, fragMod, nullptr );
        }

        {
            const VkShaderModule vertMod = VkUtil::loadSpv( device, VkUtil::resolveShaderPath( "shadow.vert.spv" ).c_str() );

            VkPipelineShaderStageCreateInfo stage{};
            stage.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            stage.stage  = VK_SHADER_STAGE_VERTEX_BIT;
            stage.module = vertMod;
            stage.pName  = "main";

            const VkVertexInputBindingDescription   binding{ 0, sizeof( float ) * 3, VK_VERTEX_INPUT_RATE_VERTEX };
            const VkVertexInputAttributeDescription attr{ 0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0 };

            VkPipelineVertexInputStateCreateInfo vertexInput{};
            vertexInput.sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
            vertexInput.vertexBindingDescriptionCount   = 1;
            vertexInput.pVertexBindingDescriptions      = &binding;
            vertexInput.vertexAttributeDescriptionCount = 1;
            vertexInput.pVertexAttributeDescriptions    = &attr;

            VkPipelineRasterizationStateCreateInfo raster{};
            raster.sType                   = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
            raster.polygonMode             = VK_POLYGON_MODE_FILL;
            raster.cullMode                = VK_CULL_MODE_NONE;
            raster.frontFace               = VK_FRONT_FACE_COUNTER_CLOCKWISE;
            raster.lineWidth               = 1.0f;
            raster.depthBiasEnable         = VK_TRUE;
            raster.depthBiasConstantFactor = 1.25f;
            raster.depthBiasSlopeFactor    = 1.75f;

            VkPipelineRenderingCreateInfo renderingInfo{};
            renderingInfo.sType                 = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
            renderingInfo.depthAttachmentFormat = VK_FORMAT_D32_SFLOAT;

            VkGraphicsPipelineCreateInfo pi{};
            pi.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
            pi.pNext               = &renderingInfo;
            pi.stageCount          = 1;
            pi.pStages             = &stage;
            pi.pVertexInputState   = &vertexInput;
            pi.pInputAssemblyState = &inputAsm;
            pi.pViewportState      = &viewportState;
            pi.pRasterizationState = &raster;
            pi.pMultisampleState   = &multisample;
            pi.pDepthStencilState  = &depthStencil;
            pi.pDynamicState       = &dynamicState;
            pi.layout              = m_shadowPipeLayout;

            if ( vkCreateGraphicsPipelines( device, VK_NULL_HANDLE, 1, &pi, nullptr, &m_shadowPipeline ) != VK_SUCCESS )
                throw std::runtime_error( "[TVkClusteredRenderer] Failed to create shadow pipeline" );

            vkDestroyShaderModule( device, vertMod, nullptr );
        }

        {
            const VkShaderModule vertMod = VkUtil::loadSpv( device, VkUtil::resolveShaderPath( "skinned_shadow.vert.spv" ).c_str() );

            VkPipelineShaderStageCreateInfo stage{};
            stage.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            stage.stage  = VK_SHADER_STAGE_VERTEX_BIT;
            stage.module = vertMod;
            stage.pName  = "main";

            const VkVertexInputBindingDescription bindings[] = {
                    { 0, sizeof( float ) * 3, VK_VERTEX_INPUT_RATE_VERTEX },
                    { 1, sizeof( uint32_t ) * 4, VK_VERTEX_INPUT_RATE_VERTEX },
                    { 2, sizeof( float ) * 4, VK_VERTEX_INPUT_RATE_VERTEX },
            };
            const VkVertexInputAttributeDescription attrs[] = {
                    { 0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0 },
                    { 1, 1, VK_FORMAT_R32G32B32A32_UINT, 0 },
                    { 2, 2, VK_FORMAT_R32G32B32A32_SFLOAT, 0 },
            };

            VkPipelineVertexInputStateCreateInfo vertexInput{};
            vertexInput.sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
            vertexInput.vertexBindingDescriptionCount   = 3;
            vertexInput.pVertexBindingDescriptions      = bindings;
            vertexInput.vertexAttributeDescriptionCount = 3;
            vertexInput.pVertexAttributeDescriptions    = attrs;

            VkPipelineRasterizationStateCreateInfo raster{};
            raster.sType                   = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
            raster.polygonMode             = VK_POLYGON_MODE_FILL;
            raster.cullMode                = VK_CULL_MODE_NONE;
            raster.frontFace               = VK_FRONT_FACE_COUNTER_CLOCKWISE;
            raster.lineWidth               = 1.0f;
            raster.depthBiasEnable         = VK_TRUE;
            raster.depthBiasConstantFactor = 1.25f;
            raster.depthBiasSlopeFactor    = 1.75f;

            VkPipelineRenderingCreateInfo renderingInfo{};
            renderingInfo.sType                 = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
            renderingInfo.depthAttachmentFormat = VK_FORMAT_D32_SFLOAT;

            VkGraphicsPipelineCreateInfo pi{};
            pi.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
            pi.pNext               = &renderingInfo;
            pi.stageCount          = 1;
            pi.pStages             = &stage;
            pi.pVertexInputState   = &vertexInput;
            pi.pInputAssemblyState = &inputAsm;
            pi.pViewportState      = &viewportState;
            pi.pRasterizationState = &raster;
            pi.pMultisampleState   = &multisample;
            pi.pDepthStencilState  = &depthStencil;
            pi.pDynamicState       = &dynamicState;
            pi.layout              = m_shadowPipeLayout;

            if ( vkCreateGraphicsPipelines( device, VK_NULL_HANDLE, 1, &pi, nullptr, &m_skinnedShadowPipeline ) != VK_SUCCESS )
                throw std::runtime_error( "[TVkClusteredRenderer] Failed to create skinned shadow pipeline" );

            vkDestroyShaderModule( device, vertMod, nullptr );
        }
    }

}  // namespace Tomos
