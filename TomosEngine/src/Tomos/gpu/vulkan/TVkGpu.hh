#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <array>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "Tomos/gpu/vulkan/TRenderDestination.hh"
#include "Tomos/gpu/vulkan/TVkBuffer.hh"
#include "Tomos/gpu/vulkan/TVkImage.hh"
#include "Tomos/gpu/vulkan/TVkPass.hh"
#include "Tomos/gpu/vulkan/TVkUtil.hh"

namespace Tomos
{
    class TVkClusteredRenderer;

    inline constexpr uint32_t g_kFramesInFlight = 3;

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

    // One in-flight transfer submission. Staging buffers stay alive until the
    // slot's fence signals, so batches never block the render thread.
    struct TVkUploadSlot
    {
        VkCommandBuffer        m_cmd      = VK_NULL_HANDLE;
        VkFence                m_fence    = VK_NULL_HANDLE;
        bool                   m_inFlight = false;
        std::vector<TVkBuffer> m_staging;
    };

    class TVkGpu
    {
    public:
        explicit TVkGpu( GLFWwindow* p_window, bool p_validation = true );
        ~TVkGpu();

        TVkGpu( const TVkGpu& )            = delete;
        TVkGpu& operator=( const TVkGpu& ) = delete;

        // User-facing swapchain present policy → VkPresentModeKHR with fallbacks.
        enum class TSwapchainPresentPolicy
        {
            Fifo,
            Mailbox,
            Immediate,
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

        [[nodiscard]] bool               tonemapTargetsSwapchain() const { return m_renderDestination.m_targetsSwapchain; }
        [[nodiscard]] TRenderDestination resolvedRenderDestination() const;

        void setRenderDestination( const TRenderDestination& p_dest, bool p_waitIdle = true );
        void resetRenderDestinationToSwapchain( bool p_waitIdle = true );
        void updateOffscreenDestination();

        [[nodiscard]] TRenderDestination makeSceneColorDestination() const;

        [[nodiscard]] TSwapchainPresentPolicy swapchainPresentPolicy() const { return m_presentPolicy; }
        void                                  setSwapchainPresentPolicy( TSwapchainPresentPolicy p_policy );
        [[nodiscard]] VkPresentModeKHR        activePresentMode() const { return m_activePresentMode; }

        static TSwapchainPresentPolicy presentPolicyFromString( const std::string& p_s );
        static const char*             presentPolicyToString( TSwapchainPresentPolicy p_policy );
        static const char*             vkPresentModeName( VkPresentModeKHR p_mode );

        // Called after a successful swapchain recreate (e.g. ImGui SetMinImageCount).
        void setSwapchainRebuildHook( std::function<void( uint32_t )> p_hook ) { m_swapchainRebuildHook = std::move( p_hook ); }

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

        // Record many copies into one submit; staging lives until the slot's fence
        // signals. Nested uploadBuffer/uploadImage while a batch is open queue into it.
        // endUploadBatch() submits without waiting — same-queue submission order
        // keeps the transfers visible to the frame's draws.
        void beginUploadBatch();
        void endUploadBatch();

        void uploadBuffer( TVkBuffer& p_dst, const void* p_data, size_t p_size );
        void uploadImage( TVkImage& p_dst, const void* p_pixels, uint32_t p_width, uint32_t p_height );

        // Re-upload mip 0 of an existing sampled image (SHADER_READ_ONLY → TRANSFER_DST → copy → SHADER_READ_ONLY).
        // Requires mipLevels == 1 (animated textures). Batches with beginUploadBatch when open.
        void updateImage( TVkImage& p_dst, const void* p_pixels, uint32_t p_width, uint32_t p_height );

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
        void               createUploadRing();
        void               destroyUploadRing();
        // Blocks only when the slot is still in flight; also releases its staging.
        void waitUploadSlot( TVkUploadSlot& p_slot );
        // Non-blocking: frees staging for slots whose fence already signalled.
        void reclaimUploadSlots();
        [[nodiscard]] bool framebufferSizeDrifted() const;
        [[nodiscard]] VkExtent2D resolveSwapchainExtent( const VkSurfaceCapabilitiesKHR& p_caps ) const;
        [[nodiscard]] VkPresentModeKHR choosePresentMode( const std::vector<VkPresentModeKHR>& p_modes ) const;
        void                             applyRenderExtentForDestination();
        void                             syncSwapchainDestinationFields();

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
        TRenderDestination       m_renderDestination{};

        TSwapchainPresentPolicy m_presentPolicy      = TSwapchainPresentPolicy::Mailbox;
        VkPresentModeKHR        m_activePresentMode  = VK_PRESENT_MODE_FIFO_KHR;
        std::function<void( uint32_t )> m_swapchainRebuildHook;

        std::array<TVkFrameData, g_kFramesInFlight> m_frames{};
        uint32_t                                    m_frameIndex = 0;
        uint32_t                                    m_imageIndex = 0;
        bool                                        m_frameOpen  = false;

        VkCommandPool                                m_uploadPool = VK_NULL_HANDLE;
        std::array<TVkUploadSlot, g_kFramesInFlight> m_uploadSlots{};
        uint32_t                                     m_uploadSlotIndex = 0;
        VkCommandBuffer                              m_batchCmd        = VK_NULL_HANDLE;
        bool                                         m_batchOpen       = false;
        VkDescriptorPool                             m_descPool        = VK_NULL_HANDLE;
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
