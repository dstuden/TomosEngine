#include "Tomos/ui/TImGuiBackend.hh"

#include <GLFW/glfw3.h>
#include <algorithm>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_vulkan.h>
#include <cassert>
#include <imgui.h>
#include <stdexcept>

#include "Tomos/gpu/vulkan/TVkGpu.hh"

namespace Tomos
{
    bool TImGuiBackend::s_withinUiFrame = false;

    void TImGuiBackend::assertWithinUiFrame() { assert( s_withinUiFrame && "ImGui only between TUiLayer newFrame() and render() — use onUi()" ); }

    void TImGuiBackend::init( TVkGpu& p_gpu, GLFWwindow* p_window )
    {
        m_gpu    = &p_gpu;
        m_window = p_window;

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();

        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        ImGui::StyleColorsDark();

        // install_callbacks = true: ImGui chains onto the callbacks TWindow
        // installed, so both the engine event system and ImGui receive input.
        ImGui_ImplGlfw_InitForVulkan( p_window, true );

        const VkFormat colorFormat = p_gpu.swapFormat();

        VkPipelineRenderingCreateInfo renderingInfo{};
        renderingInfo.sType                   = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
        renderingInfo.colorAttachmentCount    = 1;
        renderingInfo.pColorAttachmentFormats = &colorFormat;

        ImGui_ImplVulkan_InitInfo info{};
        info.Instance                    = p_gpu.instance();
        info.PhysicalDevice              = p_gpu.physDevice();
        info.Device                      = p_gpu.device();
        info.QueueFamily                 = p_gpu.graphicsFamily();
        info.Queue                       = p_gpu.graphicsQueue();
        info.DescriptorPool              = p_gpu.descPool();
        info.MinImageCount               = std::min( k_framesInFlight, p_gpu.swapImageCount() );
        info.ImageCount                  = p_gpu.swapImageCount();
        info.MSAASamples                 = VK_SAMPLE_COUNT_1_BIT;
        info.UseDynamicRendering         = true;
        info.PipelineRenderingCreateInfo = renderingInfo;

        if ( !ImGui_ImplVulkan_Init( &info ) ) throw std::runtime_error( "[TImGuiBackend] ImGui_ImplVulkan_Init failed" );

        m_initialized = true;
    }

    void TImGuiBackend::shutdown()
    {
        if ( !m_initialized ) return;

        // Caller must already have waitIdle'd — font atlas / pipelines are destroyed here.
        ImGui_ImplVulkan_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();

        s_withinUiFrame = false;
        m_window        = nullptr;
        m_gpu           = nullptr;
        m_initialized   = false;
    }

    void TImGuiBackend::newFrame()
    {
        if ( !m_initialized ) return;

        // Cursor-disabled (fly-cam) still feeds a virtual mouse position into
        // GLFW — without this, ImGui keeps hovering / clicking under the look.
        ImGuiIO& io = ImGui::GetIO();
        if ( m_window != nullptr && glfwGetInputMode( m_window, GLFW_CURSOR ) == GLFW_CURSOR_DISABLED )
            io.ConfigFlags |= ImGuiConfigFlags_NoMouse;
        else
            io.ConfigFlags &= ~ImGuiConfigFlags_NoMouse;

        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        s_withinUiFrame = true;
    }

    void TImGuiBackend::render( VkCommandBuffer p_cmd )
    {
        if ( !m_initialized ) return;

        s_withinUiFrame = false;
        ImGui::Render();
        ImGui_ImplVulkan_RenderDrawData( ImGui::GetDrawData(), p_cmd );
    }

    void TImGuiBackend::abandonFrame()
    {
        if ( !m_initialized || !s_withinUiFrame ) return;
        s_withinUiFrame = false;
        ImGui::EndFrame();
    }

    void* TImGuiBackend::registerTexture( VkSampler p_sampler, VkImageView p_view, VkImageLayout p_layout )
    {
        return reinterpret_cast<void*>( ImGui_ImplVulkan_AddTexture( p_sampler, p_view, p_layout ) );
    }

    void TImGuiBackend::unregisterTexture( void* p_texId )
    {
        if ( p_texId == nullptr ) return;
        ImGui_ImplVulkan_RemoveTexture( reinterpret_cast<VkDescriptorSet>( p_texId ) );
    }

    bool TImGuiBackend::capturingUi()
    {
        if ( ImGui::GetCurrentContext() == nullptr ) return false;

        const ImGuiIO& io = ImGui::GetIO();
        return io.WantCaptureMouse || io.WantCaptureKeyboard || ImGui::IsWindowFocused( ImGuiFocusedFlags_AnyWindow ) || ImGui::IsAnyItemActive();
    }

    bool TImGuiBackend::wantsMouse() const
    {
        if ( !m_initialized ) return false;
        // Fly-cam cursor: do not consume mouse events for the game.
        if ( ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_NoMouse ) return false;
        return capturingUi();
    }

    bool TImGuiBackend::wantsKeyboard() const { return m_initialized && capturingUi(); }

    bool TImGuiBackend::ioWantsMouse() { return capturingUi() && !( ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_NoMouse ); }

    bool TImGuiBackend::ioWantsKeyboard() { return capturingUi(); }

    bool TImGuiBackend::ioBlocksGameInput() { return ioWantsMouse(); }

    bool TImGuiBackend::ioWantTextInput() { return ImGui::GetCurrentContext() != nullptr && ImGui::GetIO().WantTextInput; }
}  // namespace Tomos
