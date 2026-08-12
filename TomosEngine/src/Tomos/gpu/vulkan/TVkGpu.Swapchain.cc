#define GLFW_INCLUDE_VULKAN
#include "Tomos/gpu/vulkan/TVkGpu.hh"

#include <GLFW/glfw3.h>
#include <algorithm>
#include <stdexcept>
#include <vector>

#include "Tomos/gpu/vulkan/renderer/TVkClusteredRenderer.hh"

namespace Tomos
{
    void TVkGpu::createSwapchain()
    {
        VkSurfaceCapabilitiesKHR caps{};
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR( m_physDevice, m_surface, &caps );

        uint32_t formatCount = 0;
        vkGetPhysicalDeviceSurfaceFormatsKHR( m_physDevice, m_surface, &formatCount, nullptr );
        std::vector<VkSurfaceFormatKHR> formats( formatCount );
        vkGetPhysicalDeviceSurfaceFormatsKHR( m_physDevice, m_surface, &formatCount, formats.data() );

        VkSurfaceFormatKHR chosen = formats[ 0 ];
        for ( const auto& f : formats )
        {
            if ( f.format == VK_FORMAT_B8G8R8A8_SRGB && f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR )
            {
                chosen = f;
                break;
            }
        }
        m_swapFormat = chosen.format;

        uint32_t modeCount = 0;
        vkGetPhysicalDeviceSurfacePresentModesKHR( m_physDevice, m_surface, &modeCount, nullptr );
        std::vector<VkPresentModeKHR> modes( modeCount );
        vkGetPhysicalDeviceSurfacePresentModesKHR( m_physDevice, m_surface, &modeCount, modes.data() );

        VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;
        for ( const auto& m : modes )
        {
            if ( m == VK_PRESENT_MODE_MAILBOX_KHR )
            {
                presentMode = m;
                break;
            }
        }

        if ( caps.currentExtent.width != UINT32_MAX )
        {
            m_extent = caps.currentExtent;
        }
        else
        {
            int fbW = 0, fbH = 0;
            glfwGetFramebufferSize( m_window, &fbW, &fbH );
            m_extent.width  = std::clamp( static_cast<uint32_t>( fbW ), caps.minImageExtent.width, caps.maxImageExtent.width );
            m_extent.height = std::clamp( static_cast<uint32_t>( fbH ), caps.minImageExtent.height, caps.maxImageExtent.height );
        }

        uint32_t imageCount = std::max( caps.minImageCount + 1, k_framesInFlight );
        if ( caps.maxImageCount > 0 ) imageCount = std::min( imageCount, caps.maxImageCount );

        const uint32_t queueFamilies[] = { m_graphicsFamily, m_presentFamily };
        const bool     sharedQueue     = ( m_graphicsFamily == m_presentFamily );

        VkSwapchainCreateInfoKHR createInfo{};
        createInfo.sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        createInfo.surface          = m_surface;
        createInfo.minImageCount    = imageCount;
        createInfo.imageFormat      = chosen.format;
        createInfo.imageColorSpace  = chosen.colorSpace;
        createInfo.imageExtent      = m_extent;
        createInfo.imageArrayLayers = 1;
        createInfo.imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        createInfo.preTransform     = caps.currentTransform;
        createInfo.compositeAlpha   = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        createInfo.presentMode      = presentMode;
        createInfo.clipped          = VK_TRUE;

        if ( sharedQueue )
        {
            createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        }
        else
        {
            createInfo.imageSharingMode      = VK_SHARING_MODE_CONCURRENT;
            createInfo.queueFamilyIndexCount = 2;
            createInfo.pQueueFamilyIndices   = queueFamilies;
        }

        if ( vkCreateSwapchainKHR( m_device, &createInfo, nullptr, &m_swapchain ) != VK_SUCCESS )
            throw std::runtime_error( "[TVkGpu] Failed to create swapchain" );

        uint32_t swapCount = 0;
        vkGetSwapchainImagesKHR( m_device, m_swapchain, &swapCount, nullptr );
        m_swapImages.resize( swapCount );
        vkGetSwapchainImagesKHR( m_device, m_swapchain, &swapCount, m_swapImages.data() );

        m_swapViews.resize( swapCount );
        for ( uint32_t i = 0; i < swapCount; ++i )
        {
            VkImageViewCreateInfo viewInfo{};
            viewInfo.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            viewInfo.image                           = m_swapImages[ i ];
            viewInfo.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
            viewInfo.format                          = m_swapFormat;
            viewInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
            viewInfo.subresourceRange.baseMipLevel   = 0;
            viewInfo.subresourceRange.levelCount     = 1;
            viewInfo.subresourceRange.baseArrayLayer = 0;
            viewInfo.subresourceRange.layerCount     = 1;
            vkCreateImageView( m_device, &viewInfo, nullptr, &m_swapViews[ i ] );
        }

        m_renderDoneSems.resize( swapCount );
        VkSemaphoreCreateInfo semInfo{ VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
        for ( uint32_t i = 0; i < swapCount; ++i ) vkCreateSemaphore( m_device, &semInfo, nullptr, &m_renderDoneSems[ i ] );
    }

    void TVkGpu::destroySwapchain()
    {
        for ( auto sem : m_renderDoneSems ) vkDestroySemaphore( m_device, sem, nullptr );
        m_renderDoneSems.clear();

        for ( auto view : m_swapViews ) vkDestroyImageView( m_device, view, nullptr );
        m_swapViews.clear();
        m_swapImages.clear();
        if ( m_swapchain != VK_NULL_HANDLE )
        {
            vkDestroySwapchainKHR( m_device, m_swapchain, nullptr );
            m_swapchain = VK_NULL_HANDLE;
        }
    }

    void TVkGpu::rebuildSwapchain()
    {
        // Wait until the window is restored (Wayland / minimize can report 0×0).
        int fbW = 0, fbH = 0;
        glfwGetFramebufferSize( m_window, &fbW, &fbH );
        while ( fbW == 0 || fbH == 0 )
        {
            glfwGetFramebufferSize( m_window, &fbW, &fbH );
            glfwWaitEvents();
        }

        destroySwapchain();
        createSwapchain();
        if ( m_presentMode == TPresentMode::Swapchain ) m_renderExtent = m_extent;
        if ( m_renderer ) m_renderer->onResize();
        m_swapchainDirty = false;
    }

    bool TVkGpu::framebufferSizeDrifted() const
    {
        int fbW = 0, fbH = 0;
        glfwGetFramebufferSize( m_window, &fbW, &fbH );
        return fbW > 0 && fbH > 0 && ( static_cast<uint32_t>( fbW ) != m_extent.width || static_cast<uint32_t>( fbH ) != m_extent.height );
    }

    void TVkGpu::requestRenderExtent( uint32_t p_width, uint32_t p_height )
    {
        p_width  = std::max( 1u, std::min( p_width, 8192u ) );
        p_height = std::max( 1u, std::min( p_height, 8192u ) );
        if ( p_width == m_renderExtent.width && p_height == m_renderExtent.height )
        {
            m_renderExtentDirty   = false;
            m_pendingRenderWidth  = 0;
            m_pendingRenderHeight = 0;
            return;
        }
        m_pendingRenderWidth  = p_width;
        m_pendingRenderHeight = p_height;
        m_renderExtentDirty   = true;
    }

    void TVkGpu::flushPendingResizes()
    {
        if ( framebufferSizeDrifted() ) m_swapchainDirty = true;
        if ( !m_swapchainDirty && !m_renderExtentDirty ) return;

        // Device idle once for both swapchain and render-target recreate.
        waitIdle();

        if ( m_swapchainDirty ) rebuildSwapchain();

        if ( m_renderExtentDirty )
        {
            m_renderExtent        = { m_pendingRenderWidth, m_pendingRenderHeight };
            m_renderExtentDirty   = false;
            m_pendingRenderWidth  = 0;
            m_pendingRenderHeight = 0;
            if ( m_renderer ) m_renderer->onRenderExtentChanged( m_renderExtent );
        }
    }

    void TVkGpu::setPresentMode( TPresentMode p_mode, bool p_waitIdle )
    {
        if ( m_presentMode == p_mode ) return;
        if ( p_waitIdle ) waitIdle();
        m_presentMode = p_mode;
        if ( m_presentMode == TPresentMode::Swapchain )
        {
            m_renderExtent = m_extent;
            if ( m_renderer ) m_renderer->onRenderExtentChanged( m_renderExtent );
        }
        else if ( m_renderer && ( m_renderExtent.width == 0 || m_renderExtent.height == 0 ) )
        {
            m_renderExtent = m_extent;
            m_renderer->onRenderExtentChanged( m_renderExtent );
        }
    }

    VkImageView TVkGpu::sceneColorView() const { return m_renderer ? m_renderer->sceneColorView() : VK_NULL_HANDLE; }

    VkSampler TVkGpu::sceneColorSampler() const { return m_renderer ? m_renderer->sceneColorSampler() : VK_NULL_HANDLE; }

    bool TVkGpu::sceneColorReady() const { return m_renderer != nullptr && m_renderer->sceneColorReady(); }

    uint32_t TVkGpu::sceneColorGeneration() const { return m_renderer ? m_renderer->sceneColorGeneration() : 0; }

}  // namespace Tomos
