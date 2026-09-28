#include "Tomos/gpu/vulkan/TVkImage.hh"

#include <algorithm>
#include <stdexcept>

#include "Tomos/gpu/vulkan/TVkUtil.hh"

namespace Tomos
{
    VkFormat TVkImage::toVkFormat( TImgFormat p_format )
    {
        switch ( p_format )
        {
            case TImgFormat::RGBA8Unorm:
                return VK_FORMAT_R8G8B8A8_UNORM;
            case TImgFormat::RGBA8Srgb:
                return VK_FORMAT_R8G8B8A8_SRGB;
            case TImgFormat::RGBA16Float:
                return VK_FORMAT_R16G16B16A16_SFLOAT;
            case TImgFormat::RG8Unorm:
                return VK_FORMAT_R8G8_UNORM;
            case TImgFormat::R8Unorm:
                return VK_FORMAT_R8_UNORM;
            case TImgFormat::R32Float:
                return VK_FORMAT_R32_SFLOAT;
            case TImgFormat::B8G8R8A8Unorm:
                return VK_FORMAT_B8G8R8A8_UNORM;
            case TImgFormat::B8G8R8A8Srgb:
                return VK_FORMAT_B8G8R8A8_SRGB;
            case TImgFormat::D32Float:
                return VK_FORMAT_D32_SFLOAT;
            case TImgFormat::D24UnormS8Uint:
                return VK_FORMAT_D24_UNORM_S8_UINT;
            case TImgFormat::B10G11R11UFloat:
                return VK_FORMAT_B10G11R11_UFLOAT_PACK32;
        }
        return VK_FORMAT_UNDEFINED;
    }

    TVkImage::TVkImage( VkDevice p_device, VkPhysicalDevice p_physDevice, const TVkImageDesc& p_desc ) :
        m_device( p_device ), m_vkFormat( toVkFormat( p_desc.m_format ) ), m_width( p_desc.m_width ), m_height( p_desc.m_height ), m_layers( p_desc.m_layers ),
        m_mipLevels( std::max( 1u, p_desc.m_mipLevels ) )
    {
        const bool isDepth = ( p_desc.m_format == TImgFormat::D32Float || p_desc.m_format == TImgFormat::D24UnormS8Uint );

        VkImageUsageFlags imgUsage = 0;
        if ( p_desc.m_usage & TImgUsage::CopySrc ) imgUsage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        if ( p_desc.m_usage & TImgUsage::CopyDst ) imgUsage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        if ( p_desc.m_usage & TImgUsage::Sampled ) imgUsage |= VK_IMAGE_USAGE_SAMPLED_BIT;
        if ( p_desc.m_usage & TImgUsage::ColorAttachment ) imgUsage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        if ( p_desc.m_usage & TImgUsage::DepthAttachment ) imgUsage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        if ( p_desc.m_usage & TImgUsage::Storage ) imgUsage |= VK_IMAGE_USAGE_STORAGE_BIT;

        VkImageCreateInfo imgInfo{};
        imgInfo.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imgInfo.imageType     = VK_IMAGE_TYPE_2D;
        imgInfo.format        = m_vkFormat;
        imgInfo.extent        = { m_width, m_height, 1 };
        imgInfo.mipLevels     = m_mipLevels;
        imgInfo.arrayLayers   = p_desc.m_layers;
        imgInfo.samples       = VK_SAMPLE_COUNT_1_BIT;
        imgInfo.tiling        = VK_IMAGE_TILING_OPTIMAL;
        imgInfo.usage         = imgUsage;
        imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        if ( vkCreateImage( m_device, &imgInfo, nullptr, &m_image ) != VK_SUCCESS ) throw std::runtime_error( "[TVkImage] Failed to create VkImage" );

        VkMemoryRequirements memReq{};
        vkGetImageMemoryRequirements( m_device, m_image, &memReq );

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize  = memReq.size;
        allocInfo.memoryTypeIndex = VkUtil::findMemoryType( p_physDevice, memReq.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );

        if ( vkAllocateMemory( m_device, &allocInfo, nullptr, &m_memory ) != VK_SUCCESS )
            throw std::runtime_error( "[TVkImage] Failed to allocate image memory" );

        vkBindImageMemory( m_device, m_image, m_memory, 0 );

        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image                           = m_image;
        viewInfo.viewType                        = ( p_desc.m_layers > 1 ) ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format                          = m_vkFormat;
        viewInfo.subresourceRange.aspectMask     = isDepth ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel   = 0;
        viewInfo.subresourceRange.levelCount     = m_mipLevels;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount     = p_desc.m_layers;

        if ( vkCreateImageView( m_device, &viewInfo, nullptr, &m_view ) != VK_SUCCESS ) throw std::runtime_error( "[TVkImage] Failed to create VkImageView" );

        if ( p_desc.m_sampled )
        {
            const VkFilter filter = ( p_desc.m_filter == TTexFilter::Linear ) ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;

            VkSamplerAddressMode addr{};
            switch ( p_desc.m_addr )
            {
                case TTexAddr::Mirror:
                    addr = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
                    break;
                case TTexAddr::Repeat:
                    addr = VK_SAMPLER_ADDRESS_MODE_REPEAT;
                    break;
                default:
                    addr = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
                    break;
            }

            VkSamplerCreateInfo samplerInfo{};
            samplerInfo.sType            = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
            samplerInfo.magFilter        = filter;
            samplerInfo.minFilter        = filter;
            samplerInfo.mipmapMode       = VK_SAMPLER_MIPMAP_MODE_LINEAR;
            samplerInfo.addressModeU     = addr;
            samplerInfo.addressModeV     = addr;
            samplerInfo.addressModeW     = addr;
            samplerInfo.maxLod           = static_cast<float>( m_mipLevels );
            samplerInfo.anisotropyEnable = VK_FALSE;

            if ( p_desc.m_shadow )
            {
                // Comparison sampler for PCF shadow lookup
                samplerInfo.compareEnable = VK_TRUE;
                samplerInfo.compareOp     = VK_COMPARE_OP_LESS_OR_EQUAL;
            }

            if ( vkCreateSampler( m_device, &samplerInfo, nullptr, &m_sampler ) != VK_SUCCESS )
                throw std::runtime_error( "[TVkImage] Failed to create VkSampler" );
        }
    }

    TVkImage::~TVkImage()
    {
        if ( m_device == VK_NULL_HANDLE ) return;
        if ( m_sampler != VK_NULL_HANDLE ) vkDestroySampler( m_device, m_sampler, nullptr );
        if ( m_view != VK_NULL_HANDLE ) vkDestroyImageView( m_device, m_view, nullptr );
        if ( m_image != VK_NULL_HANDLE ) vkDestroyImage( m_device, m_image, nullptr );
        if ( m_memory != VK_NULL_HANDLE ) vkFreeMemory( m_device, m_memory, nullptr );
    }

    TVkImage::TVkImage( TVkImage&& p_other ) noexcept :
        m_device( p_other.m_device ), m_image( p_other.m_image ), m_memory( p_other.m_memory ), m_view( p_other.m_view ), m_sampler( p_other.m_sampler ),
        m_vkFormat( p_other.m_vkFormat ), m_width( p_other.m_width ), m_height( p_other.m_height ), m_layers( p_other.m_layers ),
        m_mipLevels( p_other.m_mipLevels )
    {
        p_other.m_device    = VK_NULL_HANDLE;
        p_other.m_image     = VK_NULL_HANDLE;
        p_other.m_memory    = VK_NULL_HANDLE;
        p_other.m_view      = VK_NULL_HANDLE;
        p_other.m_sampler   = VK_NULL_HANDLE;
        p_other.m_mipLevels = 1;
    }

    TVkImage& TVkImage::operator=( TVkImage&& p_other ) noexcept
    {
        if ( this == &p_other ) return *this;
        this->~TVkImage();
        new ( this ) TVkImage( std::move( p_other ) );
        return *this;
    }
}  // namespace Tomos
