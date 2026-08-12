#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <array>
#include <memory>
#include <string>
#include <vector>

#include "Tomos/gpu/vulkan/TVkBuffer.hh"
#include "Tomos/gpu/vulkan/TVkImage.hh"
#include "Tomos/gpu/vulkan/TVkPass.hh"
#include "Tomos/gpu/vulkan/TVkUtil.hh"

namespace Tomos
{
    class TVkClusteredRenderer;

    // Triple-buffered frame concurrency.
    inline constexpr uint32_t k_framesInFlight = 3;

    struct TVkLayouts
    {
        VkDescriptorSetLayout m_material = VK_NULL_HANDLE;
    };

    struct TVkFrameData
    {
        VkCommandPool   m_cmdPool  = VK_NULL_HANDLE;
        VkCommandBuffer m_cmd      = VK_NULL_HANDLE;
        VkSemaphore     m_imgReady = VK_NULL_HANDLE;
        VkFence         m_fence    = VK_NULL_HANDLE;

        TVkBuffer m_instanceBuf;
        TVkBuffer m_lightBuf;
        TVkBuffer m_spriteBuf;
        TVkBuffer m_boneBuf;
        TVkBuffer m_sceneUBO;
    };

    // Owns device/swapchain/frame plumbing; rendering delegated to TVkClusteredRenderer.
    class TVkGpu
    {
    public:
        explicit TVkGpu( GLFWwindow* p_window, bool p_validation = true );
        ~TVkGpu();

        TVkGpu( const TVkGpu& )            = delete;
        TVkGpu& operator=( const TVkGpu& ) = delete;

        // Where tonemap writes the final LDR image.
        enum class TPresentMode
        {
            Swapchain,
            EditorViewport,
        };

        [[nodiscard]] uint32_t viewportWidth() const { return m_renderExtent.width; }
        [[nodiscard]] uint32_t viewportHeight() const { return m_renderExtent.height; }
        [[nodiscard]] float    aspectRatio() const
        {
            return m_renderExtent.height > 0 ? static_cast<float>( m_renderExtent.width ) / static_cast<float>( m_renderExtent.height ) : 1.0f;
        }

        void startFrame();
        void render();
        void endFrame();

        void waitIdle() const { vkDeviceWaitIdle( m_device ); }

        [[nodiscard]] VkDevice          device() const { return m_device; }
        [[nodiscard]] VkPhysicalDevice  physDevice() const { return m_physDevice; }
        [[nodiscard]] VkInstance        instance() const { return m_instance; }
        [[nodiscard]] VkQueue           graphicsQueue() const { return m_graphicsQueue; }
        [[nodiscard]] uint32_t          graphicsFamily() const { return m_graphicsFamily; }
        [[nodiscard]] uint32_t          swapImageCount() const { return static_cast<uint32_t>( m_swapImages.size() ); }
        [[nodiscard]] VkDescriptorPool  descPool() const { return m_descPool; }
        [[nodiscard]] const TVkLayouts& layouts() const { return m_layouts; }
        [[nodiscard]] VkExtent2D        extent() const { return m_extent; }
        [[nodiscard]] VkExtent2D        renderExtent() const { return m_renderExtent; }
        [[nodiscard]] float             renderAspectRatio() const { return aspectRatio(); }
        [[nodiscard]] VkFormat          swapFormat() const { return m_swapFormat; }

        [[nodiscard]] TPresentMode presentMode() const { return m_presentMode; }
        // When p_waitIdle is false, device must already be idle (teardown paths).
        void setPresentMode( TPresentMode p_mode, bool p_waitIdle = true );

        // Cheap; never recreate from event callbacks or while a frame is open.
        void noteSurfaceResized() { m_swapchainDirty = true; }

        // Applied in startFrame() after waitIdle. Keep ImGui scene descriptors
        // alive until flush; rebind afterward.
        void requestRenderExtent( uint32_t p_width, uint32_t p_height );

        [[nodiscard]] VkImageView sceneColorView() const;
        [[nodiscard]] VkSampler   sceneColorSampler() const;
        [[nodiscard]] bool        sceneColorReady() const;
        [[nodiscard]] uint32_t    sceneColorGeneration() const;

        [[nodiscard]] TVkFrameData& currentFrame() { return m_frames[ m_frameIndex ]; }
        [[nodiscard]] TVkFrameData& frameData( uint32_t p_index ) { return m_frames[ p_index ]; }
        [[nodiscard]] VkImage       currentSwapImage() const { return m_swapImages[ m_imageIndex ]; }
        [[nodiscard]] VkImageView   currentSwapView() const { return m_swapViews[ m_imageIndex ]; }

        [[nodiscard]] TFrameState& frameState() { return m_frameState; }

        // Overlays must skip recording when false (e.g. swapchain just rebuilt).
        [[nodiscard]] bool frameOpen() const { return m_frameOpen; }

        void immediateSubmit( const auto& p_fn ) const { VkUtil::immediateSubmit( m_device, m_uploadPool, m_graphicsQueue, p_fn ); }

        void uploadBuffer( TVkBuffer& p_dst, const void* p_data, size_t p_size );
        void uploadImage( TVkImage& p_dst, const void* p_pixels, uint32_t p_width, uint32_t p_height );

        [[nodiscard]] const TVkImage& defaultTexture() const { return m_defaultTexture; }
        // Device sentinel — not bag-owned.
        [[nodiscard]] const TVkImage& missingTexture() const { return m_missingTexture; }

        [[nodiscard]] TVkClusteredRenderer* renderer() const { return m_renderer.get(); }

    private:
        void               createInstance();
        void               createDebugMessenger();
        void               createSurface( GLFWwindow* p_window );
        void               selectPhysicalDevice();
        void               createLogicalDevice();
        void               createSwapchain();
        void               createFrameData();
        void               createDescriptorPool();
        void               createLayouts();
        void               destroySwapchain();
        void               rebuildSwapchain();
        void               flushPendingResizes();
        void               uploadFrameState( TVkFrameData& p_frame );
        [[nodiscard]] bool framebufferSizeDrifted() const;

        static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback( VkDebugUtilsMessageSeverityFlagBitsEXT, VkDebugUtilsMessageTypeFlagsEXT,
                                                             const VkDebugUtilsMessengerCallbackDataEXT*, void* );

        GLFWwindow*              m_window         = nullptr;
        VkInstance               m_instance       = VK_NULL_HANDLE;
        VkDebugUtilsMessengerEXT m_debugMessenger = VK_NULL_HANDLE;
        VkSurfaceKHR             m_surface        = VK_NULL_HANDLE;
        VkPhysicalDevice         m_physDevice     = VK_NULL_HANDLE;
        VkDevice                 m_device         = VK_NULL_HANDLE;
        VkQueue                  m_graphicsQueue  = VK_NULL_HANDLE;
        VkQueue                  m_presentQueue   = VK_NULL_HANDLE;
        uint32_t                 m_graphicsFamily = 0;
        uint32_t                 m_presentFamily  = 0;

        VkSwapchainKHR           m_swapchain = VK_NULL_HANDLE;
        std::vector<VkImage>     m_swapImages;
        std::vector<VkImageView> m_swapViews;
        VkFormat                 m_swapFormat = VK_FORMAT_UNDEFINED;
        VkExtent2D               m_extent{};
        VkExtent2D               m_renderExtent{};  // may differ from swap (HDR/post/camera)
        TPresentMode             m_presentMode = TPresentMode::Swapchain;

        std::array<TVkFrameData, k_framesInFlight> m_frames{};
        uint32_t                                   m_frameIndex = 0;
        uint32_t                                   m_imageIndex = 0;
        bool                                       m_frameOpen  = false;

        VkCommandPool            m_uploadPool = VK_NULL_HANDLE;
        VkDescriptorPool         m_descPool   = VK_NULL_HANDLE;
        TVkLayouts               m_layouts{};
        std::vector<VkSemaphore> m_renderDoneSems;

        std::unique_ptr<TVkClusteredRenderer> m_renderer;

        TFrameState m_frameState;
        TVkImage    m_defaultTexture;
        TVkImage    m_missingTexture;

        bool     m_validation          = false;
        bool     m_swapchainDirty      = false;
        bool     m_renderExtentDirty   = false;
        uint32_t m_pendingRenderWidth  = 0;
        uint32_t m_pendingRenderHeight = 0;
    };
}  // namespace Tomos
