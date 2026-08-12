#pragma once

#include <array>

#include "Tomos/gpu/vulkan/TVkImage.hh"
#include "Tomos/gpu/vulkan/post/TPostEffect.hh"

namespace Tomos
{
    class TPostSSAO : public TPostEffect
    {
    public:
        TPostSSAO() { m_enabled = true; }
        ~TPostSSAO() override { destroy(); }

        [[nodiscard]] const char* name() const override { return "SSAO"; }

        void onResize( const TPostContext& p_ctx ) override;
        void reloadShaders( const TPostContext& p_ctx ) override;
        void record( VkCommandBuffer p_cmd, TPostContext& p_ctx ) override;
        void destroy() override;

        // World-space sample radius (meters).  Scaled to UV in the shader so AO
        // does not grow with distance (fixed-UV kernels cause dark banding).
        float m_radius    = 0.35f;
        float m_bias      = 0.03f;
        float m_intensity = 1.0f;

    private:
        static constexpr uint32_t k_frames = 3;

        VkDevice m_device = VK_NULL_HANDLE;

        TVkImage m_ao;

        VkDescriptorSetLayout                 m_samp1Layout = VK_NULL_HANDLE;
        VkDescriptorSetLayout                 m_samp2Layout = VK_NULL_HANDLE;
        VkPipelineLayout                      m_aoLay       = VK_NULL_HANDLE;
        VkPipelineLayout                      m_compLay     = VK_NULL_HANDLE;
        VkPipeline                            m_aoPipe      = VK_NULL_HANDLE;
        VkPipeline                            m_compPipe    = VK_NULL_HANDLE;
        std::array<VkDescriptorSet, k_frames> m_aoSets{};
        std::array<VkDescriptorSet, k_frames> m_compSets{};
    };
}  // namespace Tomos
