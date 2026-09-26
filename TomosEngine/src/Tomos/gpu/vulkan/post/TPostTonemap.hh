#pragma once

#include <array>

#include "Tomos/gpu/vulkan/post/TPostEffect.hh"

namespace Tomos
{
    class TPostTonemap : public TPostEffect
    {
    public:
        TPostTonemap() { m_enabled = true; }
        ~TPostTonemap() override { destroy(); }

        [[nodiscard]] const char* name() const override { return "Tonemap"; }

        void onResize( const TPostContext& p_ctx ) override;
        void record( VkCommandBuffer p_cmd, TPostContext& p_ctx ) override;
        void destroy() override;

    private:
        static constexpr uint32_t g_kFrames = 3;

        VkDevice                               m_device  = VK_NULL_HANDLE;
        VkDescriptorSetLayout                  m_layout  = VK_NULL_HANDLE;
        VkPipelineLayout                       m_pipeLay = VK_NULL_HANDLE;
        VkPipeline                             m_pipe    = VK_NULL_HANDLE;
        std::array<VkDescriptorSet, g_kFrames> m_sets{};
    };
}  // namespace Tomos
