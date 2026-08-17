#pragma once

#include <array>

#include "Tomos/gpu/vulkan/TVkImage.hh"
#include "Tomos/gpu/vulkan/post/TPostEffect.hh"

namespace Tomos
{
    class TPostBloom : public TPostEffect
    {
    public:
        TPostBloom() { m_enabled = true; }
        ~TPostBloom() override { destroy(); }

        [[nodiscard]] const char* name() const override { return "Bloom"; }

        void onResize( const TPostContext& p_ctx ) override;
        void reloadShaders( const TPostContext& p_ctx ) override;
        void record( VkCommandBuffer p_cmd, TPostContext& p_ctx ) override;
        void destroy() override;

        float m_threshold = 1.0f;
        float m_strength  = 0.35f;

    private:
        void ensurePipelines( const TPostContext& p_ctx );

        static constexpr uint32_t g_kFrames = 3;

        VkDevice m_device = VK_NULL_HANDLE;

        TVkImage m_brightA;
        TVkImage m_brightB;

        VkDescriptorSetLayout m_samp1Layout = VK_NULL_HANDLE;
        VkDescriptorSetLayout m_samp2Layout = VK_NULL_HANDLE;
        VkPipelineLayout      m_extractLay  = VK_NULL_HANDLE;
        VkPipelineLayout      m_blurLay     = VK_NULL_HANDLE;
        VkPipelineLayout      m_compLay     = VK_NULL_HANDLE;
        VkPipeline            m_extractPipe = VK_NULL_HANDLE;
        VkPipeline            m_blurPipe    = VK_NULL_HANDLE;
        VkPipeline            m_compPipe    = VK_NULL_HANDLE;

        // Per frames-in-flight; blur uses two sets (H then V) within one frame.
        std::array<VkDescriptorSet, g_kFrames> m_extractSets{};
        std::array<VkDescriptorSet, g_kFrames> m_blurHSets{};
        std::array<VkDescriptorSet, g_kFrames> m_blurVSets{};
        std::array<VkDescriptorSet, g_kFrames> m_compSets{};

        VkExtent2D m_half{};
    };
}  // namespace Tomos
