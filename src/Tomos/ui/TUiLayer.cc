#include "Tomos/ui/TUiLayer.hh"

#include <cassert>
#include <stdexcept>

#include "Tomos/core/app/TApplication.hh"
#include "Tomos/gpu/vulkan/TVkGpu.hh"
#include "Tomos/ui/TImGuiBackend.hh"

namespace Tomos
{
    TUiLayer::TUiLayer( std::unique_ptr<TUiBackend> p_backend, std::string p_name ) : TLayer( std::move( p_name ) ), m_backend( std::move( p_backend ) ) {}

    void TUiLayer::onAttach()
    {
        TApplication& app = TApplication::get();
        TVkGpu*       gpu = app.gpu();
        if ( gpu == nullptr ) throw std::runtime_error( "[TUiLayer] GPU is null on attach" );
        m_backend->init( *gpu, app.window().getNativeWindow() );
    }

    void TUiLayer::onDetach()
    {
        // One idle before tearing down UI Vulkan objects (font atlas, pipelines).
        if ( auto* gpu = TApplication::get().gpu() ) gpu->waitIdle();
        m_backend->shutdown();
    }

    void TUiLayer::onUpdate( float p_dt )
    {
        // Overlays update after scene layers, so the UI frame spans
        // newFrame() → render() within this same engine frame.
        m_backend->newFrame();
        // ImGui widgets are only valid between newFrame() and render().
        // Subclasses must build UI in onUi() — never from a scene layer.
        assert( !dynamic_cast<TImGuiBackend*>( m_backend.get() ) || TImGuiBackend::withinUiFrame() );
        onUi( p_dt );
    }

    void TUiLayer::onRender()
    {
        TVkGpu* gpu = TApplication::get().gpu();
        // startFrame() may skip (OUT_OF_DATE after fullscreen) — still close the
        // UI frame opened in onUpdate or the next NewFrame asserts.
        if ( gpu == nullptr || !gpu->frameOpen() )
        {
            m_backend->abandonFrame();
            return;
        }

        const VkCommandBuffer cmd = gpu->currentFrame().m_cmd;
        m_backend->prepareRender( cmd );

        const bool tonemapToSwapchain = gpu->tonemapTargetsSwapchain();

        VkRenderingAttachmentInfo colorAtt{};
        colorAtt.sType       = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        colorAtt.imageView   = gpu->currentSwapView();
        colorAtt.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        colorAtt.loadOp      = tonemapToSwapchain ? VK_ATTACHMENT_LOAD_OP_LOAD : VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAtt.storeOp     = VK_ATTACHMENT_STORE_OP_STORE;
        if ( !tonemapToSwapchain )
        {
            colorAtt.clearValue.color.float32[ 0 ] = 0.12f;
            colorAtt.clearValue.color.float32[ 1 ] = 0.12f;
            colorAtt.clearValue.color.float32[ 2 ] = 0.14f;
            colorAtt.clearValue.color.float32[ 3 ] = 1.0f;

            // Tonemap skipped swapchain — present may still be UNDEFINED first use.
            VkImageMemoryBarrier2 barrier{};
            barrier.sType            = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
            barrier.srcStageMask     = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
            barrier.srcAccessMask    = VK_ACCESS_2_NONE;
            barrier.dstStageMask     = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
            barrier.dstAccessMask    = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
            barrier.oldLayout        = VK_IMAGE_LAYOUT_UNDEFINED;
            barrier.newLayout        = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            barrier.image            = gpu->currentSwapImage();
            barrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

            VkDependencyInfo dep{};
            dep.sType                   = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
            dep.imageMemoryBarrierCount = 1;
            dep.pImageMemoryBarriers    = &barrier;
            vkCmdPipelineBarrier2( cmd, &dep );
        }

        VkRenderingInfo rendering{};
        rendering.sType                = VK_STRUCTURE_TYPE_RENDERING_INFO;
        rendering.renderArea.extent    = gpu->extent();
        rendering.layerCount           = 1;
        rendering.colorAttachmentCount = 1;
        rendering.pColorAttachments    = &colorAtt;

        vkCmdBeginRendering( cmd, &rendering );
        m_backend->render( cmd );
        vkCmdEndRendering( cmd );
    }

    void TUiLayer::onEvent( TEvent& p_event )
    {
        // When the UI is hovered / focused, swallow all INPUT events (keys + mouse)
        // so gameplay layers never see them. Application events still propagate.
        if ( ( m_backend->wantsMouse() || m_backend->wantsKeyboard() ) && p_event.isInCategory( TEventCategory::INPUT ) )
        {
            p_event.setHandled();
        }
    }
}  // namespace Tomos
