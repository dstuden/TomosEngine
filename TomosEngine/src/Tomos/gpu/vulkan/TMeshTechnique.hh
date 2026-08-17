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
        VkPipeline m_opaque        = VK_NULL_HANDLE;
        VkPipeline m_blend         = VK_NULL_HANDLE;
        VkPipeline m_skinnedOpaque = VK_NULL_HANDLE;
        VkPipeline m_skinnedBlend  = VK_NULL_HANDLE;

        [[nodiscard]] VkPipeline pick( bool p_blend, bool p_skinned ) const
        {
            if ( p_skinned ) return p_blend ? m_skinnedBlend : m_skinnedOpaque;
            return p_blend ? m_blend : m_opaque;
        }

        void destroyAll( VkDevice p_device )
        {
            auto destroy = [ & ]( VkPipeline& p_p )
            {
                if ( p_p != VK_NULL_HANDLE )
                {
                    vkDestroyPipeline( p_device, p_p, nullptr );
                    p_p = VK_NULL_HANDLE;
                }
            };
            destroy( m_opaque );
            destroy( m_blend );
            destroy( m_skinnedOpaque );
            destroy( m_skinnedBlend );
        }
    };

    struct TMeshTechniqueCreateCtx
    {
        VkDevice                                      m_device        = VK_NULL_HANDLE;
        VkPipelineLayout                              m_layout        = VK_NULL_HANDLE;
        const VkPipelineInputAssemblyStateCreateInfo* m_inputAsm      = nullptr;
        const VkPipelineViewportStateCreateInfo*      m_viewportState = nullptr;
        const VkPipelineMultisampleStateCreateInfo*   m_multisample   = nullptr;
        const VkPipelineDynamicStateCreateInfo*       m_dynamicState  = nullptr;
    };

    struct TMeshTechniqueCreateDesc
    {
        const char* m_staticVertSpv  = nullptr;
        const char* m_skinnedVertSpv = nullptr;
        const char* m_fragSpv        = nullptr;
        const char* m_label          = "mesh";
        bool        m_createOpaque   = true;
        bool        m_createBlend    = true;
    };

    void createMeshTechniquePipelines( const TMeshTechniqueCreateCtx& p_ctx, const TMeshTechniqueCreateDesc& p_desc, TMeshTechniquePipelines& p_out );
}  // namespace Tomos
