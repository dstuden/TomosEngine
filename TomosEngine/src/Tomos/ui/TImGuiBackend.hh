#pragma once

#include <vulkan/vulkan.h>

#include "Tomos/ui/TUiBackend.hh"

struct ImTextureData;

namespace Tomos
{
    class TImGuiBackend : public TUiBackend
    {
    public:
        TImGuiBackend()           = default;
        ~TImGuiBackend() override = default;

        void init( TVkGpu& p_gpu, GLFWwindow* p_window ) override;
        void shutdown() override;
        void newFrame() override;
        void render( VkCommandBuffer p_cmd ) override;
        void abandonFrame() override;

        [[nodiscard]] bool wantsMouse() const override;
        [[nodiscard]] bool wantsKeyboard() const override;

        [[nodiscard]] static bool withinUiFrame() { return g_sWithinUiFrame; }
        static void               assertWithinUiFrame();

        [[nodiscard]] static bool ioWantsMouse();
        [[nodiscard]] static bool ioWantsKeyboard();
        [[nodiscard]] static bool ioBlocksGameInput();
        [[nodiscard]] static bool ioWantTextInput();

        [[nodiscard]] static void* registerTexture( VkSampler p_sampler, VkImageView p_view,
                                                    VkImageLayout p_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL );
        static void                unregisterTexture( void* p_texId );

    private:
        [[nodiscard]] static bool capturingUi();

        static bool g_sWithinUiFrame;

        TVkGpu*     m_gpu         = nullptr;
        GLFWwindow* m_window      = nullptr;
        bool        m_initialized = false;
    };
}  // namespace Tomos
