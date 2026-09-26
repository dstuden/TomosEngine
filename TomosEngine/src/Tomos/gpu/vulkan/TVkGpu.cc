#define GLFW_INCLUDE_VULKAN
#include "Tomos/gpu/vulkan/TVkGpu.hh"

#include <GLFW/glfw3.h>
#include <algorithm>
#include <cstring>
#include <set>
#include <stdexcept>
#include <vector>

#include "Tomos/gpu/vulkan/TVkUtil.hh"
#include "Tomos/gpu/vulkan/renderer/TVkClusteredRenderer.hh"

namespace Tomos
{
    TVkGpu::TVkGpu( GLFWwindow* p_window, bool p_validation ) : m_window( p_window ), m_validation( p_validation )
    {
        createInstance();
        if ( m_validation ) createDebugMessenger();
        createSurface( p_window );
        selectPhysicalDevice();
        createLogicalDevice();
        createSwapchain();
        syncSwapchainDestinationFields();
        createFrameData();
        createDescriptorPool();
        createLayouts();

        // Neutral 1×1 white — intentional "no texture" fills (optional material
        // maps, tinted sprites, soft particles).
        {
            TVkImageDesc desc{};
            desc.m_width             = 1;
            desc.m_height            = 1;
            desc.m_format            = TImgFormat::RGBA8Unorm;
            desc.m_usage             = TImgUsage::CopyDst | TImgUsage::Sampled;
            desc.m_sampled           = true;
            m_defaultTexture         = TVkImage( m_device, m_physDevice, desc );
            const uint8_t white[ 4 ] = { 255, 255, 255, 255 };
            uploadImage( m_defaultTexture, white, 1, 1 );
        }

        // Classic black / magenta checkerboard for textures that failed to load.
        {
            constexpr uint32_t kSize = 64;
            constexpr uint32_t kTile = 8;
            TVkImageDesc       desc{};
            desc.m_width     = kSize;
            desc.m_height    = kSize;
            desc.m_format    = TImgFormat::RGBA8Unorm;
            desc.m_usage     = TImgUsage::CopyDst | TImgUsage::Sampled;
            desc.m_sampled   = true;
            desc.m_addr      = TTexAddr::Repeat;
            m_missingTexture = TVkImage( m_device, m_physDevice, desc );

            std::vector<uint8_t> pixels( kSize * kSize * 4 );
            for ( uint32_t y = 0; y < kSize; ++y )
            {
                for ( uint32_t x = 0; x < kSize; ++x )
                {
                    const bool     magenta = ( ( x / kTile ) + ( y / kTile ) ) % 2 == 0;
                    const uint32_t i       = ( y * kSize + x ) * 4;
                    pixels[ i + 0 ]        = magenta ? 255 : 0;
                    pixels[ i + 1 ]        = 0;
                    pixels[ i + 2 ]        = magenta ? 255 : 0;
                    pixels[ i + 3 ]        = 255;
                }
            }
            uploadImage( m_missingTexture, pixels.data(), kSize, kSize );
        }

        m_renderer = std::make_unique<TVkClusteredRenderer>( *this );
    }

    float TVkGpu::timestampPeriod() const
    {
        if ( m_physDevice == VK_NULL_HANDLE ) return 1.0f;
        VkPhysicalDeviceProperties props{};
        vkGetPhysicalDeviceProperties( m_physDevice, &props );
        return props.limits.timestampPeriod;
    }

    TVkGpu::~TVkGpu()
    {
        if ( m_device != VK_NULL_HANDLE ) vkDeviceWaitIdle( m_device );

        // Drop upload staging first — buffers may host-unmap during destruction.
        if ( m_batchOpen ) endUploadBatch();
        destroyUploadRing();

        m_renderer.reset();

        for ( auto& frame : m_frames )
        {
            frame.m_instanceBuf = TVkBuffer{};
            frame.m_lightBuf    = TVkBuffer{};
            frame.m_spriteBuf       = TVkBuffer{};
            frame.m_boneBuf         = TVkBuffer{};
            frame.m_sceneUBO        = TVkBuffer{};

            vkDestroySemaphore( m_device, frame.m_imgReady, nullptr );
            vkDestroyFence( m_device, frame.m_fence, nullptr );
            vkDestroyCommandPool( m_device, frame.m_cmdPool, nullptr );
        }

        m_defaultTexture = TVkImage{};
        m_missingTexture = TVkImage{};

        vkDestroyDescriptorSetLayout( m_device, m_layouts.m_material, nullptr );

        vkDestroyDescriptorPool( m_device, m_descPool, nullptr );

        destroySwapchain();

        if ( m_uploadPool != VK_NULL_HANDLE ) vkDestroyCommandPool( m_device, m_uploadPool, nullptr );
        vkDestroyDevice( m_device, nullptr );
        m_device = VK_NULL_HANDLE;

        if ( m_instance != VK_NULL_HANDLE && m_surface != VK_NULL_HANDLE ) vkDestroySurfaceKHR( m_instance, m_surface, nullptr );
        m_surface = VK_NULL_HANDLE;

        if ( m_debugMessenger != VK_NULL_HANDLE )
        {
            auto fn = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>( vkGetInstanceProcAddr( m_instance, "vkDestroyDebugUtilsMessengerEXT" ) );
            if ( fn ) fn( m_instance, m_debugMessenger, nullptr );
        }

        vkDestroyInstance( m_instance, nullptr );
    }

    void TVkGpu::startFrame()
    {
        m_frameOpen = false;
        reclaimUploadSlots();
        TVkFrameData& frame = m_frames[ m_frameIndex ];

        vkWaitForFences( m_device, 1, &frame.m_fence, VK_TRUE, UINT64_MAX );

        // Apply deferred swapchain / render-extent changes after the prior frame
        // has finished (fence waited) and before acquire / recording.
        flushPendingResizes();

        const VkResult result = vkAcquireNextImageKHR( m_device, m_swapchain, UINT64_MAX, frame.m_imgReady, VK_NULL_HANDLE, &m_imageIndex );
        // OUT_OF_DATE: acquire failed — semaphore is NOT signaled. Rebuild and skip.
        // SUBOPTIMAL: image WAS acquired and semaphore WILL signal — must continue
        // this frame (or wait on the semaphore). Never rebuild+return here or the
        // next acquire hangs ("Semaphore must not have any pending operations").
        if ( result == VK_ERROR_OUT_OF_DATE_KHR )
        {
            m_swapchainDirty = true;
            flushPendingResizes();
            return;
        }
        if ( result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR ) return;
        if ( result == VK_SUBOPTIMAL_KHR ) m_swapchainDirty = true;

        vkResetFences( m_device, 1, &frame.m_fence );
        vkResetCommandPool( m_device, frame.m_cmdPool, 0 );

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer( frame.m_cmd, &beginInfo );

        m_frameOpen = true;
    }

    void TVkGpu::render()
    {
        if ( !m_frameOpen ) return;

        // Populate fills frameState in onRender (after post-update TRS). Upload here
        // so SSBOs match this frame's draws — still just host memcpy, no GPU idle.
        uploadFrameState( m_frames[ m_frameIndex ] );
        m_renderer->render( m_frames[ m_frameIndex ].m_cmd, m_frameIndex, m_frameState );
    }

    void TVkGpu::endFrame()
    {
        if ( !m_frameOpen ) return;

        TVkFrameData& frame = m_frames[ m_frameIndex ];

        VkUtil::imageBarrier( frame.m_cmd, m_swapImages[ m_imageIndex ], VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                              VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT,
                              VK_ACCESS_2_NONE );

        vkEndCommandBuffer( frame.m_cmd );

        const VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

        VkSubmitInfo submitInfo{};
        submitInfo.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.waitSemaphoreCount   = 1;
        submitInfo.pWaitSemaphores      = &frame.m_imgReady;
        submitInfo.pWaitDstStageMask    = &waitStage;
        submitInfo.commandBufferCount   = 1;
        submitInfo.pCommandBuffers      = &frame.m_cmd;
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores    = &m_renderDoneSems[ m_imageIndex ];

        vkQueueSubmit( m_graphicsQueue, 1, &submitInfo, frame.m_fence );

        VkPresentInfoKHR presentInfo{};
        presentInfo.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores    = &m_renderDoneSems[ m_imageIndex ];
        presentInfo.swapchainCount     = 1;
        presentInfo.pSwapchains        = &m_swapchain;
        presentInfo.pImageIndices      = &m_imageIndex;

        const VkResult result = vkQueuePresentKHR( m_presentQueue, &presentInfo );
        // Defer rebuild — do not waitIdle while this frame's fence is still the
        // in-flight marker for the just-submitted work (next startFrame flushes).
        if ( result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR ) m_swapchainDirty = true;

        m_frameIndex = ( m_frameIndex + 1 ) % g_kFramesInFlight;
        m_frameOpen  = false;
    }

    void TVkGpu::createInstance()
    {
        VkApplicationInfo appInfo{};
        appInfo.sType      = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.apiVersion = VK_API_VERSION_1_3;

        uint32_t     glfwExtCount = 0;
        const char** glfwExts     = glfwGetRequiredInstanceExtensions( &glfwExtCount );

        std::vector<const char*> extensions( glfwExts, glfwExts + glfwExtCount );
        if ( m_validation ) extensions.push_back( VK_EXT_DEBUG_UTILS_EXTENSION_NAME );

        const char* validationLayer = "VK_LAYER_KHRONOS_validation";

        VkInstanceCreateInfo createInfo{};
        createInfo.sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo        = &appInfo;
        createInfo.enabledExtensionCount   = static_cast<uint32_t>( extensions.size() );
        createInfo.ppEnabledExtensionNames = extensions.data();

        if ( m_validation )
        {
            createInfo.enabledLayerCount   = 1;
            createInfo.ppEnabledLayerNames = &validationLayer;
        }

        if ( vkCreateInstance( &createInfo, nullptr, &m_instance ) != VK_SUCCESS ) throw std::runtime_error( "[TVkGpu] Failed to create VkInstance" );
    }

    void TVkGpu::createDebugMessenger()
    {
        VkDebugUtilsMessengerCreateInfoEXT ci{};
        ci.sType           = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        ci.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        ci.messageType     = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;
        ci.pfnUserCallback = debugCallback;

        auto fn = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>( vkGetInstanceProcAddr( m_instance, "vkCreateDebugUtilsMessengerEXT" ) );
        if ( !fn || fn( m_instance, &ci, nullptr, &m_debugMessenger ) != VK_SUCCESS ) throw std::runtime_error( "[TVkGpu] Failed to create debug messenger" );
    }

    void TVkGpu::createSurface( GLFWwindow* p_window )
    {
        if ( glfwCreateWindowSurface( m_instance, p_window, nullptr, &m_surface ) != VK_SUCCESS )
            throw std::runtime_error( "[TVkGpu] Failed to create window surface" );
    }

    void TVkGpu::selectPhysicalDevice()
    {
        uint32_t count = 0;
        vkEnumeratePhysicalDevices( m_instance, &count, nullptr );
        if ( count == 0 ) throw std::runtime_error( "[TVkGpu] No Vulkan-capable GPUs found" );

        std::vector<VkPhysicalDevice> devices( count );
        vkEnumeratePhysicalDevices( m_instance, &count, devices.data() );

        VkPhysicalDevice best      = VK_NULL_HANDLE;
        int              bestScore = -1;

        for ( VkPhysicalDevice dev : devices )
        {
            VkPhysicalDeviceProperties props{};
            vkGetPhysicalDeviceProperties( dev, &props );

            if ( props.apiVersion < VK_API_VERSION_1_3 ) continue;

            uint32_t qCount = 0;
            vkGetPhysicalDeviceQueueFamilyProperties( dev, &qCount, nullptr );
            std::vector<VkQueueFamilyProperties> qProps( qCount );
            vkGetPhysicalDeviceQueueFamilyProperties( dev, &qCount, qProps.data() );

            uint32_t gfx     = UINT32_MAX;
            uint32_t present = UINT32_MAX;
            for ( uint32_t i = 0; i < qCount; ++i )
            {
                if ( qProps[ i ].queueFlags & VK_QUEUE_GRAPHICS_BIT ) gfx = i;
                VkBool32 canPresent = VK_FALSE;
                vkGetPhysicalDeviceSurfaceSupportKHR( dev, i, m_surface, &canPresent );
                if ( canPresent ) present = i;
            }
            if ( gfx == UINT32_MAX || present == UINT32_MAX ) continue;

            VkPhysicalDeviceVulkan13Features feat13{};
            feat13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
            VkPhysicalDeviceFeatures2 feat2{};
            feat2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
            feat2.pNext = &feat13;
            vkGetPhysicalDeviceFeatures2( dev, &feat2 );

            if ( !feat13.dynamicRendering || !feat13.synchronization2 ) continue;

            int score = 0;
            if ( props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU )
                score = 1000;
            else if ( props.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU )
                score = 100;

            if ( score > bestScore )
            {
                bestScore        = score;
                best             = dev;
                m_graphicsFamily = gfx;
                m_presentFamily  = present;
            }
        }

        if ( best == VK_NULL_HANDLE )
            throw std::runtime_error(
                    "[TVkGpu] No suitable physical device found (requires Vulkan 1.3 + dynamicRendering + "
                    "synchronization2)" );

        m_physDevice = best;
    }

    void TVkGpu::createLogicalDevice()
    {
        const float              priority       = 1.0f;
        const std::set<uint32_t> uniqueFamilies = { m_graphicsFamily, m_presentFamily };

        std::vector<VkDeviceQueueCreateInfo> queueInfos;
        for ( uint32_t family : uniqueFamilies )
        {
            VkDeviceQueueCreateInfo qi{};
            qi.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            qi.queueFamilyIndex = family;
            qi.queueCount       = 1;
            qi.pQueuePriorities = &priority;
            queueInfos.push_back( qi );
        }

        VkPhysicalDeviceVulkan13Features feat13{};
        feat13.sType            = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
        feat13.dynamicRendering = VK_TRUE;
        feat13.synchronization2 = VK_TRUE;

        VkPhysicalDeviceFeatures2 feat2{};
        feat2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
        feat2.pNext = &feat13;

        const char* swapchainExt = VK_KHR_SWAPCHAIN_EXTENSION_NAME;

        VkDeviceCreateInfo createInfo{};
        createInfo.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        createInfo.pNext                   = &feat2;
        createInfo.queueCreateInfoCount    = static_cast<uint32_t>( queueInfos.size() );
        createInfo.pQueueCreateInfos       = queueInfos.data();
        createInfo.enabledExtensionCount   = 1;
        createInfo.ppEnabledExtensionNames = &swapchainExt;

        if ( vkCreateDevice( m_physDevice, &createInfo, nullptr, &m_device ) != VK_SUCCESS )
            throw std::runtime_error( "[TVkGpu] Failed to create logical device" );

        vkGetDeviceQueue( m_device, m_graphicsFamily, 0, &m_graphicsQueue );
        vkGetDeviceQueue( m_device, m_presentFamily, 0, &m_presentQueue );

        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.queueFamilyIndex = m_graphicsFamily;
        poolInfo.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        vkCreateCommandPool( m_device, &poolInfo, nullptr, &m_uploadPool );

        createUploadRing();
    }


    void TVkGpu::createFrameData()
    {
        for ( uint32_t i = 0; i < g_kFramesInFlight; ++i )
        {
            TVkFrameData& frame = m_frames[ i ];

            VkCommandPoolCreateInfo poolInfo{};
            poolInfo.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
            poolInfo.queueFamilyIndex = m_graphicsFamily;
            vkCreateCommandPool( m_device, &poolInfo, nullptr, &frame.m_cmdPool );

            VkCommandBufferAllocateInfo cmdInfo{};
            cmdInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
            cmdInfo.commandPool        = frame.m_cmdPool;
            cmdInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
            cmdInfo.commandBufferCount = 1;
            vkAllocateCommandBuffers( m_device, &cmdInfo, &frame.m_cmd );

            VkSemaphoreCreateInfo semInfo{ VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
            vkCreateSemaphore( m_device, &semInfo, nullptr, &frame.m_imgReady );

            VkFenceCreateInfo fenceInfo{ VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
            fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
            vkCreateFence( m_device, &fenceInfo, nullptr, &frame.m_fence );

            frame.m_instanceBuf = TVkBuffer( m_device, m_physDevice, g_kMaxInstances * sizeof( TInstanceData ), TBufUsage::Storage );
            frame.m_lightBuf    = TVkBuffer( m_device, m_physDevice, g_kMaxLights * sizeof( TLightData ), TBufUsage::Storage );
            frame.m_spriteBuf   = TVkBuffer( m_device, m_physDevice, g_kMaxSprites * sizeof( TSpriteData ), TBufUsage::Storage );
            frame.m_boneBuf     = TVkBuffer( m_device, m_physDevice, g_kMaxBonesPerFrame * sizeof( glm::mat4 ), TBufUsage::Storage );
            frame.m_sceneUBO    = TVkBuffer( m_device, m_physDevice, sizeof( TSceneUBO ), TBufUsage::Uniform );
        }
    }

    void TVkGpu::createDescriptorPool()
    {
        const std::array<VkDescriptorPoolSize, 3> sizes{ {
                { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1024 },
                { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 256 },
                { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4096 },
        } };

        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.flags         = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        poolInfo.maxSets       = 2048;
        poolInfo.poolSizeCount = static_cast<uint32_t>( sizes.size() );
        poolInfo.pPoolSizes    = sizes.data();

        if ( vkCreateDescriptorPool( m_device, &poolInfo, nullptr, &m_descPool ) != VK_SUCCESS )
            throw std::runtime_error( "[TVkGpu] Failed to create descriptor pool" );
    }

    void TVkGpu::createLayouts()
    {
        auto makeLayout = [ & ]( const std::vector<VkDescriptorSetLayoutBinding>& p_bindings ) -> VkDescriptorSetLayout
        {
            VkDescriptorSetLayoutCreateInfo ci{};
            ci.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
            ci.bindingCount = static_cast<uint32_t>( p_bindings.size() );
            ci.pBindings    = p_bindings.data();
            VkDescriptorSetLayout layout{};
            if ( vkCreateDescriptorSetLayout( m_device, &ci, nullptr, &layout ) != VK_SUCCESS )
                throw std::runtime_error( "[TVkGpu] Failed to create descriptor set layout" );
            return layout;
        };

        m_layouts.m_material = makeLayout( {
                { 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr },
                { 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr },
                { 2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr },
                { 3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr },
                { 4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr },
        } );
    }

    VKAPI_ATTR VkBool32 VKAPI_CALL TVkGpu::debugCallback( VkDebugUtilsMessageSeverityFlagBitsEXT      p_severity, VkDebugUtilsMessageTypeFlagsEXT,
                                                          const VkDebugUtilsMessengerCallbackDataEXT* p_data, void* )
    {
        if ( p_severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT )
            fprintf( stderr, "[Vulkan ERROR] %s\n", p_data->pMessage );
        else
            fprintf( stderr, "[Vulkan WARN]  %s\n", p_data->pMessage );
        return VK_FALSE;
    }
}  // namespace Tomos
