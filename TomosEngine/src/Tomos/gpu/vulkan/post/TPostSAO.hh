#pragma once

#include <array>

#include "Tomos/gpu/vulkan/TVkImage.hh"
#include "Tomos/gpu/vulkan/post/TPostEffect.hh"

namespace Tomos
{
    // McGuire 2012 Scalable Ambient Obscurance (core: no z-MIP hierarchy).
    class TPostSAO : public TPostEffect
    {
    public:
        TPostSAO() { m_enabled = true; }
        ~TPostSAO() override { destroy(); }

        [[nodiscard]] const char* name() const override { return "SAO"; }

        void onResize( const TPostContext& p_ctx ) override;
        void record( VkCommandBuffer p_cmd, TPostContext& p_ctx ) override;
        void destroy() override;

        float m_radius         = 0.28f;
        float m_bias           = 0.03f;
        float m_intensity      = 0.55f;
        int   m_sampleCount    = 12;
        float m_blurSharpness  = 100.0f;
        float m_temporalBlend  = 0.45f;

    private:
        void ensurePipelines( const TPostContext& p_ctx );

        static constexpr uint32_t g_kFrames = 3;

        VkDevice m_device = VK_NULL_HANDLE;

        TVkImage m_linearZ;
        TVkImage m_aoPacked;
        TVkImage m_aoBlurA;
        TVkImage m_aoBlurB;
        TVkImage m_aoHist;
        bool     m_histValid = false;

        VkDescriptorSetLayout m_samp1Layout   = VK_NULL_HANDLE;
        VkDescriptorSetLayout m_samp2Layout   = VK_NULL_HANDLE;
        VkPipelineLayout      m_linLay        = VK_NULL_HANDLE;
        VkPipelineLayout      m_sampleLay     = VK_NULL_HANDLE;
        VkPipelineLayout      m_blurLay       = VK_NULL_HANDLE;
        VkPipelineLayout      m_temporalLay   = VK_NULL_HANDLE;
        VkPipelineLayout      m_compLay       = VK_NULL_HANDLE;
        VkPipeline            m_linPipe       = VK_NULL_HANDLE;
        VkPipeline            m_samplePipe    = VK_NULL_HANDLE;
        VkPipeline            m_blurPipe      = VK_NULL_HANDLE;
        VkPipeline            m_temporalPipe  = VK_NULL_HANDLE;
        VkPipeline            m_compPipe      = VK_NULL_HANDLE;

        std::array<VkDescriptorSet, g_kFrames> m_linSets{};
        std::array<VkDescriptorSet, g_kFrames> m_sampleSets{};
        // Two H+V pairs: second blur iteration must not update sets already bound in this CB.
        std::array<VkDescriptorSet, g_kFrames> m_blurHSets{};
        std::array<VkDescriptorSet, g_kFrames> m_blurVSets{};
        std::array<VkDescriptorSet, g_kFrames> m_blurHSets2{};
        std::array<VkDescriptorSet, g_kFrames> m_blurVSets2{};
        std::array<VkDescriptorSet, g_kFrames> m_temporalSets{};
        std::array<VkDescriptorSet, g_kFrames> m_compSets{};
    };
}  // namespace Tomos
