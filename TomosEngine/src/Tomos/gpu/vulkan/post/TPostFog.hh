#pragma once

#include <array>
#include <glm/glm.hpp>

#include "Tomos/gpu/vulkan/post/TPostEffect.hh"

namespace Tomos
{
    class TPostFog : public TPostEffect
    {
    public:
        TPostFog() { m_enabled = false; }
        ~TPostFog() override { destroy(); }

        [[nodiscard]] const char* name() const override { return "Fog"; }

        void onResize( const TPostContext& p_ctx ) override;
        void reloadShaders( const TPostContext& p_ctx ) override;
        void record( VkCommandBuffer p_cmd, TPostContext& p_ctx ) override;
        void destroy() override;

        glm::vec3 m_color   = { 0.55f, 0.62f, 0.72f };
        float     m_density = 0.015f;

    private:
        static constexpr uint32_t k_frames = 3;

        VkDevice                              m_device  = VK_NULL_HANDLE;
        VkDescriptorSetLayout                 m_layout  = VK_NULL_HANDLE;
        VkPipelineLayout                      m_pipeLay = VK_NULL_HANDLE;
        VkPipeline                            m_pipe    = VK_NULL_HANDLE;
        std::array<VkDescriptorSet, k_frames> m_sets{};
    };
}  // namespace Tomos
