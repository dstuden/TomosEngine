#pragma once

#include <vulkan/vulkan.h>

namespace Tomos
{
    // Tonemap output for the primary view. Swapchain fields are resolved each frame when m_targetsSwapchain.
    struct TRenderDestination
    {
        VkImage     m_image  = VK_NULL_HANDLE;
        VkImageView m_view   = VK_NULL_HANDLE;
        VkExtent2D  m_extent{};
        VkFormat    m_format = VK_FORMAT_UNDEFINED;

        bool m_sampleAfterTonemap = false;
        bool m_targetsSwapchain   = true;

        [[nodiscard]] static TRenderDestination offscreen( VkImage p_image, VkImageView p_view, VkExtent2D p_extent, VkFormat p_format,
                                                         bool p_sampleAfterTonemap = true )
        {
            TRenderDestination d{};
            d.m_targetsSwapchain   = false;
            d.m_image              = p_image;
            d.m_view               = p_view;
            d.m_extent             = p_extent;
            d.m_format             = p_format;
            d.m_sampleAfterTonemap  = p_sampleAfterTonemap;
            return d;
        }
    };
}  // namespace Tomos
