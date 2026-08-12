#pragma once

#include <vulkan/vulkan.h>

struct GLFWwindow;

namespace Tomos
{
    class TVkGpu;

    // UI backend — newFrame in onUpdate, render/abandonFrame in onRender.
    class TUiBackend
    {
    public:
        virtual ~TUiBackend() = default;

        virtual void init( TVkGpu& p_gpu, GLFWwindow* p_window ) = 0;

        virtual void shutdown() = 0;

        virtual void newFrame() = 0;

        // Optional GPU work before the UI dynamic-rendering pass.
        virtual void prepareRender( VkCommandBuffer /*p_cmd*/ ) {}

        // p_cmd is inside dynamic rendering targeting the swapchain.
        virtual void render( VkCommandBuffer p_cmd ) = 0;

        // Close newFrame without drawing (e.g. startFrame skipped OUT_OF_DATE).
        virtual void abandonFrame() = 0;

        [[nodiscard]] virtual bool wantsMouse() const { return false; }
        [[nodiscard]] virtual bool wantsKeyboard() const { return false; }
    };
}  // namespace Tomos
