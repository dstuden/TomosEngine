#pragma once

#include <array>

#include "Tomos/gpu/vulkan/TVkGpu.hh"
#include "Tomos/gpu/vulkan/TVkImage.hh"
#include "Tomos/gpu/vulkan/post/TPostEffect.hh"
#include "Tomos/util/reflect/TReflectAttr.hh"

namespace Tomos
{
    // Dual Kawase bloom: bright extract at half-res, then downsample/upsample pyramid.
    class TPostBloom : public TPostEffect
    {
    public:
        TPostBloom() { m_enabled = true; }
        ~TPostBloom() override { destroy(); }

        [[nodiscard]] const char* name() const override { return "Bloom"; }

        void onResize( const TPostContext& p_ctx ) override;
        void record( VkCommandBuffer p_cmd, TPostContext& p_ctx ) override;
        void destroy() override;

        TOMOS_ANN( Reflect::UiRange{ 0.0f, 4.0f } ) TOMOS_ANN( Reflect::UiLabel{ "Bloom threshold" } ) float m_threshold  = 1.0f;
        TOMOS_ANN( Reflect::UiRange{ 0.0f, 2.0f } ) TOMOS_ANN( Reflect::UiLabel{ "Bloom strength" } ) float  m_strength   = 0.35f;
        TOMOS_ANN( Reflect::UiRange{ 2.0f, 5.0f } ) TOMOS_ANN( Reflect::UiLabel{ "Bloom iterations" } ) int  m_iterations = 4;

    private:
        void ensurePipelines( const TPostContext& p_ctx );

        static constexpr uint32_t g_kFrames    = g_kFramesInFlight;
        static constexpr uint32_t g_kMaxLevels = 5;

        VkDevice m_device = VK_NULL_HANDLE;

        std::array<TVkImage, g_kMaxLevels> m_mips{};
        uint32_t                           m_levelCount = 0;

        VkDescriptorSetLayout m_samp1Layout = VK_NULL_HANDLE;
        VkDescriptorSetLayout m_samp2Layout = VK_NULL_HANDLE;
        VkPipelineLayout      m_extractLay  = VK_NULL_HANDLE;
        VkPipelineLayout      m_downLay     = VK_NULL_HANDLE;
        VkPipelineLayout      m_upLay       = VK_NULL_HANDLE;
        VkPipelineLayout      m_compLay     = VK_NULL_HANDLE;
        VkPipeline            m_extractPipe = VK_NULL_HANDLE;
        VkPipeline            m_downPipe    = VK_NULL_HANDLE;
        VkPipeline            m_upPipe      = VK_NULL_HANDLE;
        VkPipeline            m_compPipe    = VK_NULL_HANDLE;

        std::array<VkDescriptorSet, g_kFrames>                              m_extractSets{};
        std::array<std::array<VkDescriptorSet, g_kMaxLevels>, g_kFrames>    m_downSets{};
        std::array<std::array<VkDescriptorSet, g_kMaxLevels>, g_kFrames>    m_upSets{};
        std::array<VkDescriptorSet, g_kFrames>                              m_compSets{};
    };
}  // namespace Tomos
