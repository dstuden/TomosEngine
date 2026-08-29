#define GLFW_INCLUDE_VULKAN
#include "Tomos/gpu/vulkan/TVkGpu.hh"

#include <GLFW/glfw3.h>
#include <algorithm>
#include <stdexcept>
#include <vector>

#include "Tomos/gpu/vulkan/renderer/TVkClusteredRenderer.hh"

namespace Tomos
{
    TVkGpu::TSwapchainPresentPolicy TVkGpu::presentPolicyFromString( const std::string& p_s )
    {
        if ( p_s == "fifo" ) return TSwapchainPresentPolicy::Fifo;
        if ( p_s == "immediate" ) return TSwapchainPresentPolicy::Immediate;
        return TSwapchainPresentPolicy::Mailbox;
    }

    const char* TVkGpu::presentPolicyToString( TSwapchainPresentPolicy p_policy )
    {
        switch ( p_policy )
        {
            case TSwapchainPresentPolicy::Fifo:
                return "fifo";
            case TSwapchainPresentPolicy::Immediate:
                return "immediate";
            case TSwapchainPresentPolicy::Mailbox:
            default:
                return "mailbox";
        }
    }

    const char* TVkGpu::vkPresentModeName( VkPresentModeKHR p_mode )
    {
        switch ( p_mode )
        {
            case VK_PRESENT_MODE_IMMEDIATE_KHR:
                return "IMMEDIATE";
            case VK_PRESENT_MODE_MAILBOX_KHR:
                return "MAILBOX";
            case VK_PRESENT_MODE_FIFO_KHR:
                return "FIFO";
            case VK_PRESENT_MODE_FIFO_RELAXED_KHR:
                return "FIFO_RELAXED";
            default:
                return "UNKNOWN";
        }
    }

    VkPresentModeKHR TVkGpu::choosePresentMode( const std::vector<VkPresentModeKHR>& p_modes ) const
    {
        const auto has = [ &p_modes ]( VkPresentModeKHR m ) { return std::find( p_modes.begin(), p_modes.end(), m ) != p_modes.end(); };

        switch ( m_presentPolicy )
        {
            case TSwapchainPresentPolicy::Fifo:
                return VK_PRESENT_MODE_FIFO_KHR;
            case TSwapchainPresentPolicy::Immediate:
                if ( has( VK_PRESENT_MODE_IMMEDIATE_KHR ) ) return VK_PRESENT_MODE_IMMEDIATE_KHR;
                if ( has( VK_PRESENT_MODE_FIFO_RELAXED_KHR ) ) return VK_PRESENT_MODE_FIFO_RELAXED_KHR;
                return VK_PRESENT_MODE_FIFO_KHR;
            case TSwapchainPresentPolicy::Mailbox:
            default:
                if ( has( VK_PRESENT_MODE_MAILBOX_KHR ) ) return VK_PRESENT_MODE_MAILBOX_KHR;
                return VK_PRESENT_MODE_FIFO_KHR;
        }
    }

    VkExtent2D TVkGpu::resolveSwapchainExtent( const VkSurfaceCapabilitiesKHR& p_caps ) const
    {
        int fbW = 0, fbH = 0;
        glfwGetFramebufferSize( m_window, &fbW, &fbH );
        const uint32_t glfwW = fbW > 0 ? static_cast<uint32_t>( fbW ) : 0;
        const uint32_t glfwH = fbH > 0 ? static_cast<uint32_t>( fbH ) : 0;

        VkExtent2D extent = p_caps.currentExtent;
        const bool capsInvalid =
                extent.width == UINT32_MAX || extent.width == 0 || extent.height == 0 || extent.height == UINT32_MAX;
        const bool capsDisagree = glfwW > 0 && glfwH > 0 && ( extent.width != glfwW || extent.height != glfwH );

        if ( capsInvalid || capsDisagree )
        {
            if ( glfwW == 0 || glfwH == 0 ) return { 0, 0 };
            const uint32_t maxW = p_caps.maxImageExtent.width > 0 ? p_caps.maxImageExtent.width : glfwW;
            const uint32_t maxH = p_caps.maxImageExtent.height > 0 ? p_caps.maxImageExtent.height : glfwH;
            extent.width  = std::clamp( glfwW, p_caps.minImageExtent.width, maxW );
            extent.height = std::clamp( glfwH, p_caps.minImageExtent.height, maxH );
        }
        return extent;
    }

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

        m_activePresentMode = choosePresentMode( modes );

        m_extent = resolveSwapchainExtent( caps );
        if ( m_extent.width == 0 || m_extent.height == 0 )
            throw std::runtime_error( "[TVkGpu] Refusing to create swapchain with 0×0 extent" );

        uint32_t imageCount = std::max( caps.minImageCount + 1, g_kFramesInFlight );
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
        createInfo.presentMode      = m_activePresentMode;
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

        VkSurfaceCapabilitiesKHR caps{};
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR( m_physDevice, m_surface, &caps );
        const VkExtent2D resolved = resolveSwapchainExtent( caps );
        if ( resolved.width == 0 || resolved.height == 0 )
        {
            // Keep dirty — retry next frame. Never destroy the live swapchain for 0×0.
            m_swapchainDirty = true;
            return;
        }

        destroySwapchain();
        createSwapchain();
        if ( m_renderDestination.m_targetsSwapchain ) syncSwapchainDestinationFields();
        if ( m_renderer ) m_renderer->onResize();
        m_swapchainDirty = false;
        if ( m_swapchainRebuildHook ) m_swapchainRebuildHook( static_cast<uint32_t>( m_swapImages.size() ) );
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
            if ( !m_renderDestination.m_targetsSwapchain ) m_renderDestination.m_extent = m_renderExtent;
            if ( m_renderer ) m_renderer->onRenderExtentChanged( m_renderExtent );
        }
    }

    void TVkGpu::syncSwapchainDestinationFields()
    {
        m_renderDestination.m_targetsSwapchain = true;
        m_renderDestination.m_format           = m_swapFormat;
        m_renderDestination.m_extent           = m_extent;
        m_renderExtent                         = m_extent;
    }

    void TVkGpu::applyRenderExtentForDestination()
    {
        VkExtent2D want = m_renderDestination.m_targetsSwapchain ? m_extent : m_renderDestination.m_extent;
        if ( want.width == 0 || want.height == 0 ) want = m_extent;
        if ( want.width == m_renderExtent.width && want.height == m_renderExtent.height ) return;

        m_renderExtent = want;
        if ( m_renderer ) m_renderer->onRenderExtentChanged( m_renderExtent );
    }

    TRenderDestination TVkGpu::resolvedRenderDestination() const
    {
        TRenderDestination dest = m_renderDestination;
        if ( dest.m_targetsSwapchain )
        {
            dest.m_image              = currentSwapImage();
            dest.m_view               = currentSwapView();
            dest.m_extent             = m_extent;
            dest.m_format             = m_swapFormat;
            dest.m_sampleAfterTonemap = false;
        }
        else
        {
            dest.m_extent = m_renderExtent;
        }
        return dest;
    }

    TRenderDestination TVkGpu::makeSceneColorDestination() const
    {
        if ( m_renderer == nullptr || !m_renderer->sceneColorReady() ) return {};

        return TRenderDestination::offscreen( m_renderer->sceneColor().handle(), m_renderer->sceneColorView(), m_renderExtent, m_swapFormat );
    }

    void TVkGpu::updateOffscreenDestination()
    {
        if ( m_renderDestination.m_targetsSwapchain ) return;
        const TRenderDestination next = makeSceneColorDestination();
        if ( next.m_view == VK_NULL_HANDLE || next.m_view == m_renderDestination.m_view ) return;
        m_renderDestination.m_image = next.m_image;
        m_renderDestination.m_view  = next.m_view;
    }

    void TVkGpu::setRenderDestination( const TRenderDestination& p_dest, bool p_waitIdle )
    {
        const bool unchangedOffscreen = !p_dest.m_targetsSwapchain && !m_renderDestination.m_targetsSwapchain && p_dest.m_view == m_renderDestination.m_view &&
                                        p_dest.m_extent.width == m_renderDestination.m_extent.width && p_dest.m_extent.height == m_renderDestination.m_extent.height;
        if ( ( p_dest.m_targetsSwapchain && m_renderDestination.m_targetsSwapchain ) || unchangedOffscreen ) return;

        if ( p_waitIdle ) waitIdle();
        m_renderDestination = p_dest;
        if ( m_renderDestination.m_targetsSwapchain ) m_renderDestination.m_format = m_swapFormat;
        applyRenderExtentForDestination();
    }

    void TVkGpu::resetRenderDestinationToSwapchain( bool p_waitIdle )
    {
        TRenderDestination dest{};
        dest.m_targetsSwapchain = true;
        setRenderDestination( dest, p_waitIdle );
    }

    void TVkGpu::setSwapchainPresentPolicy( TSwapchainPresentPolicy p_policy )
    {
        if ( m_presentPolicy == p_policy ) return;
        m_presentPolicy  = p_policy;
        m_swapchainDirty = true;
    }

    VkImageView TVkGpu::sceneColorView() const { return m_renderer ? m_renderer->sceneColorView() : VK_NULL_HANDLE; }

    VkSampler TVkGpu::sceneColorSampler() const { return m_renderer ? m_renderer->sceneColorSampler() : VK_NULL_HANDLE; }

    bool TVkGpu::sceneColorReady() const { return m_renderer != nullptr && m_renderer->sceneColorReady(); }

    uint32_t TVkGpu::sceneColorGeneration() const { return m_renderer ? m_renderer->sceneColorGeneration() : 0; }

}  // namespace Tomos
