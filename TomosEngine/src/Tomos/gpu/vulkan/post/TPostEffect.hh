#pragma once

#include <glm/glm.hpp>
#include <vulkan/vulkan.h>

#include "Tomos/gpu/vulkan/TVkImage.hh"

#ifndef TOMOS_DEBUG
#define TOMOS_DEBUG 0
#endif

#if TOMOS_DEBUG
#include "Tomos/util/profile/TGpuTimestamps.hh"
#endif

namespace Tomos
{
    class TVkGpu;

    // HDR double-buffer: read ctx.hdr, write ctx.hdrOther, then swapHdr().
    struct TPostContext
    {
        TVkGpu*    m_gpu = nullptr;
        VkExtent2D m_extent{};

        TVkImage* m_hdr      = nullptr;
        TVkImage* m_hdrOther = nullptr;
        TVkImage* m_depth    = nullptr;

        float     m_near = 0.1f;
        float     m_far  = 1000.0f;
        glm::mat4 m_proj{ 1.0f };     // SAO position reconstruction (McGuire eq. 3)
        glm::mat4 m_projInv{ 1.0f };
        glm::mat4 m_viewInv{ 1.0f };

        VkImage     m_outputImage  = VK_NULL_HANDLE;
        VkImageView m_outputView   = VK_NULL_HANDLE;
        VkFormat    m_outputFormat = VK_FORMAT_UNDEFINED;

        // Only touch descriptor sets for this frame index.
        uint32_t m_frameIndex = 0;

#if TOMOS_DEBUG
        TGpuTimestamps* m_gpuTimestamps = nullptr;
#endif

        void swapHdr() { std::swap( m_hdr, m_hdrOther ); }
    };

    class TPostEffect
    {
    public:
        virtual ~TPostEffect() = default;

        [[nodiscard]] virtual const char* name() const = 0;

        virtual void onResize( const TPostContext& p_ctx ) = 0;

        virtual void record( VkCommandBuffer p_cmd, TPostContext& p_ctx ) = 0;

        virtual void destroy() {}

        bool m_enabled = true;
    };
}  // namespace Tomos
