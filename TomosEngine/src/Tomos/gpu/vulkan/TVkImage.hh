#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vulkan/vulkan.h>

#include "Tomos/gpu/TGpuEnums.hh"

namespace Tomos
{
    struct TVkImageDesc
    {
        uint32_t   m_width{};
        uint32_t   m_height{};
        uint32_t   m_layers    = 1;  // > 1 creates a 2D array image
        uint32_t   m_mipLevels = 1;
        TImgFormat m_format;
        TImgUsage  m_usage;
        TTexFilter m_filter  = TTexFilter::Linear;
        TTexAddr   m_addr    = TTexAddr::Clamp;
        bool       m_sampled = false;  // Whether to create a VkSampler
        bool       m_shadow  = false;  // Comparison sampler for shadow maps
    };

    class TVkImage
    {
    public:
        TVkImage() = default;
        TVkImage( VkDevice p_device, VkPhysicalDevice p_physDevice, const TVkImageDesc& p_desc );
        ~TVkImage();

        TVkImage( const TVkImage& )            = delete;
        TVkImage& operator=( const TVkImage& ) = delete;
        TVkImage( TVkImage&& p_other ) noexcept;
        TVkImage& operator=( TVkImage&& p_other ) noexcept;

        [[nodiscard]] VkImage     handle() const { return m_image; }
        [[nodiscard]] VkImageView view() const { return m_view; }
        [[nodiscard]] VkSampler   sampler() const { return m_sampler; }
        [[nodiscard]] VkFormat    format() const { return m_vkFormat; }
        [[nodiscard]] uint32_t    width() const { return m_width; }
        [[nodiscard]] uint32_t    height() const { return m_height; }
        [[nodiscard]] uint32_t    layers() const { return m_layers; }
        [[nodiscard]] uint32_t    mipLevels() const { return m_mipLevels; }
        [[nodiscard]] bool        valid() const { return m_image != VK_NULL_HANDLE; }

        [[nodiscard]] static VkFormat toVkFormat( TImgFormat p_format );

        // Full mip chain: floor(log2(max(w,h))) + 1.  Returns 1 for empty sizes.
        [[nodiscard]] static uint32_t calcMipLevels( uint32_t p_width, uint32_t p_height )
        {
            if ( p_width == 0 || p_height == 0 ) return 1;
            const uint32_t largest = std::max( p_width, p_height );
            return static_cast<uint32_t>( std::floor( std::log2( static_cast<double>( largest ) ) ) ) + 1u;
        }

    private:
        VkDevice       m_device    = VK_NULL_HANDLE;
        VkImage        m_image     = VK_NULL_HANDLE;
        VkDeviceMemory m_memory    = VK_NULL_HANDLE;
        VkImageView    m_view      = VK_NULL_HANDLE;
        VkSampler      m_sampler   = VK_NULL_HANDLE;
        VkFormat       m_vkFormat  = VK_FORMAT_UNDEFINED;
        uint32_t       m_width     = 0;
        uint32_t       m_height    = 0;
        uint32_t       m_layers    = 1;
        uint32_t       m_mipLevels = 1;
    };
}  // namespace Tomos
