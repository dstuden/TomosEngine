#pragma once

#include <array>

#include "Tomos/gpu/vulkan/TVkGpu.hh"
#include "Tomos/gpu/vulkan/TVkImage.hh"
#include "Tomos/gpu/vulkan/post/TPostEffect.hh"
#include "Tomos/util/reflect/TReflectAttr.hh"

namespace Tomos
{
    // McGuire 2012 Scalable Ambient Obscurance (core: no z-MIP hierarchy).
    // Runs at half resolution; compose upsamples into full HDR.
    class TPostSAO : public TPostEffect
    {
    public:
        TPostSAO() { m_enabled = true; }
        ~TPostSAO() override { destroy(); }

        [[nodiscard]] const char* name() const override { return "SAO"; }

        void onResize( const TPostContext& p_ctx ) override;
        void record( VkCommandBuffer p_cmd, TPostContext& p_ctx ) override;
        void destroy() override;

        TOMOS_ANN( Reflect::UiRange{ 0.05f, 2.0f } ) TOMOS_ANN( Reflect::UiLabel{ "SAO radius" } ) float        m_radius        = 0.28f;
        TOMOS_ANN( Reflect::UiRange{ 0.001f, 0.2f } ) TOMOS_ANN( Reflect::UiLabel{ "SAO bias" } ) float         m_bias          = 0.03f;
        TOMOS_ANN( Reflect::UiRange{ 0.0f, 2.0f } ) TOMOS_ANN( Reflect::UiLabel{ "SAO intensity" } ) float      m_intensity     = 0.55f;
        TOMOS_ANN( Reflect::UiRange{ 4.0f, 24.0f } ) TOMOS_ANN( Reflect::UiLabel{ "SAO samples" } ) int         m_sampleCount   = 12;
        TOMOS_ANN( Reflect::UiRange{ 10.0f, 500.0f } ) TOMOS_ANN( Reflect::UiLabel{ "SAO blur sharpness" } ) float m_blurSharpness = 100.0f;
        TOMOS_ANN( Reflect::UiRange{ 0.0f, 0.9f } ) TOMOS_ANN( Reflect::UiLabel{ "SAO temporal" } ) float       m_temporalBlend = 0.45f;

    private:
        void ensurePipelines( const TPostContext& p_ctx );

        static constexpr uint32_t g_kFrames = g_kFramesInFlight;

        VkDevice m_device = VK_NULL_HANDLE;

        TVkImage m_linearZ;
        TVkImage m_aoPacked;
        TVkImage m_aoBlurA;
        TVkImage m_aoBlurB;
        TVkImage m_aoHist[ 2 ];
        uint32_t m_histIndex = 0;
        bool     m_histValid = false;

        VkDescriptorSetLayout m_samp1Layout  = VK_NULL_HANDLE;
        VkDescriptorSetLayout m_samp2Layout  = VK_NULL_HANDLE;
        VkPipelineLayout      m_linLay       = VK_NULL_HANDLE;
        VkPipelineLayout      m_sampleLay    = VK_NULL_HANDLE;
        VkPipelineLayout      m_blurLay      = VK_NULL_HANDLE;
        VkPipelineLayout      m_temporalLay  = VK_NULL_HANDLE;
        VkPipelineLayout      m_compLay      = VK_NULL_HANDLE;
        VkPipeline            m_linPipe      = VK_NULL_HANDLE;
        VkPipeline            m_samplePipe   = VK_NULL_HANDLE;
        VkPipeline            m_blurPipe     = VK_NULL_HANDLE;
        VkPipeline            m_temporalPipe = VK_NULL_HANDLE;
        VkPipeline            m_compPipe     = VK_NULL_HANDLE;

        std::array<VkDescriptorSet, g_kFrames> m_linSets{};
        std::array<VkDescriptorSet, g_kFrames> m_sampleSets{};
        std::array<VkDescriptorSet, g_kFrames> m_blurHSets{};
        std::array<VkDescriptorSet, g_kFrames> m_blurVSets{};
        std::array<VkDescriptorSet, g_kFrames> m_temporalSets{};
        std::array<VkDescriptorSet, g_kFrames> m_compSets{};

        VkExtent2D m_half{};
    };
}  // namespace Tomos
