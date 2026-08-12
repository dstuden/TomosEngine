#pragma once

#include <cstdint>
#include <vulkan/vulkan.h>

namespace Tomos
{
    enum class TMeshTechniqueId : uint32_t
    {
        Forward = 0,
        Water   = 1,
        Count
    };

    // Unsupported slots stay VK_NULL_HANDLE; drawMesh skips.
    struct TMeshTechniquePipelines
    {
        VkPipeline opaque        = VK_NULL_HANDLE;
        VkPipeline blend         = VK_NULL_HANDLE;
        VkPipeline skinnedOpaque = VK_NULL_HANDLE;
        VkPipeline skinnedBlend  = VK_NULL_HANDLE;

        [[nodiscard]] VkPipeline pick( bool p_blend, bool p_skinned ) const
        {
            if ( p_skinned ) return p_blend ? skinnedBlend : skinnedOpaque;
            return p_blend ? blend : opaque;
        }

        void destroyAll( VkDevice p_device )
        {
            auto destroy = [ & ]( VkPipeline& p )
            {
                if ( p != VK_NULL_HANDLE )
                {
                    vkDestroyPipeline( p_device, p, nullptr );
                    p = VK_NULL_HANDLE;
                }
            };
            destroy( opaque );
            destroy( blend );
            destroy( skinnedOpaque );
            destroy( skinnedBlend );
        }
    };

    struct TMeshTechniqueCreateCtx
    {
        VkDevice                                       device          = VK_NULL_HANDLE;
        VkPipelineLayout                               layout          = VK_NULL_HANDLE;
        const VkPipelineInputAssemblyStateCreateInfo*  inputAsm        = nullptr;
        const VkPipelineViewportStateCreateInfo*       viewportState   = nullptr;
        const VkPipelineMultisampleStateCreateInfo*    multisample     = nullptr;
        const VkPipelineDynamicStateCreateInfo*        dynamicState    = nullptr;
    };

    struct TMeshTechniqueCreateDesc
    {
        const char* staticVertSpv  = nullptr;
        const char* skinnedVertSpv = nullptr;
        const char* fragSpv        = nullptr;
        const char* label          = "mesh";
        bool        createOpaque   = true;
        bool        createBlend    = true;
    };

    void createMeshTechniquePipelines( const TMeshTechniqueCreateCtx& p_ctx, const TMeshTechniqueCreateDesc& p_desc,
                                       TMeshTechniquePipelines& p_out );
}  // namespace Tomos
