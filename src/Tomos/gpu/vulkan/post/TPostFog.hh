#pragma once

#include <array>
#include <glm/glm.hpp>

#include "Tomos/gpu/vulkan/TVkGpu.hh"
#include "Tomos/gpu/vulkan/post/TPostEffect.hh"
#include "Tomos/util/reflect/TReflectAttr.hh"

namespace Tomos
{
    class TPostFog : public TPostEffect
    {
    public:
        TPostFog() { m_enabled = false; }
        ~TPostFog() override { destroy(); }

        [[nodiscard]] const char* name() const override { return "Fog"; }

        void onResize( const TPostContext& p_ctx ) override;
        void record( VkCommandBuffer p_cmd, TPostContext& p_ctx ) override;
        void destroy() override;

        TOMOS_ANN( Reflect::UiColor{} ) TOMOS_ANN( Reflect::UiLabel{ "Fog color" } ) glm::vec3 m_color = { 0.55f, 0.62f, 0.72f };
        TOMOS_ANN( Reflect::UiRange{ 0.0f, 0.1f } ) TOMOS_ANN( Reflect::UiLabel{ "Fog density" } ) float m_density = 0.015f;

    private:
        static constexpr uint32_t g_kFrames = g_kFramesInFlight;

        VkDevice                               m_device  = VK_NULL_HANDLE;
        VkDescriptorSetLayout                  m_layout  = VK_NULL_HANDLE;
        VkPipelineLayout                       m_pipeLay = VK_NULL_HANDLE;
        VkPipeline                             m_pipe    = VK_NULL_HANDLE;
        std::array<VkDescriptorSet, g_kFrames> m_sets{};
    };
}  // namespace Tomos
