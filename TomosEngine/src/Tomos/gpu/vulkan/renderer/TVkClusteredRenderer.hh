#pragma once

#include <array>
#include <memory>
#include <unordered_map>
#include <vulkan/vulkan.h>

#include "Tomos/gpu/vulkan/TMeshTechnique.hh"
#include "Tomos/gpu/vulkan/TVkBuffer.hh"
#include "Tomos/gpu/vulkan/TVkImage.hh"
#include "Tomos/gpu/vulkan/TVkPass.hh"
#include "Tomos/gpu/vulkan/post/TPostStack.hh"

namespace Tomos
{
    class TVkGpu;

    // Clustered forward + post stack.
    class TVkClusteredRenderer
    {
    public:
        explicit TVkClusteredRenderer( TVkGpu& p_gpu );
        ~TVkClusteredRenderer();

        TVkClusteredRenderer( const TVkClusteredRenderer& )            = delete;
        TVkClusteredRenderer& operator=( const TVkClusteredRenderer& ) = delete;

        void onResize();
        void onRenderExtentChanged( VkExtent2D p_extent );
        void render( VkCommandBuffer p_cmd, uint32_t p_frameIndex, const TFrameState& p_state );

        // Caller must vkDeviceWaitIdle first (TShaderHotReload).
        void reloadShaders();

        void uploadParticles( uint32_t p_frameIndex, const TFrameState& p_state );

        [[nodiscard]] TPostStack& postStack() { return m_post; }

        [[nodiscard]] VkExtent2D  renderExtent() const { return m_renderExtent; }
        [[nodiscard]] VkImageView sceneColorView() const { return m_sceneColor.valid() ? m_sceneColor.view() : VK_NULL_HANDLE; }
        [[nodiscard]] VkSampler   sceneColorSampler() const { return m_sceneColor.valid() ? m_sceneColor.sampler() : VK_NULL_HANDLE; }
        [[nodiscard]] bool        sceneColorReady() const { return m_sceneColor.valid(); }
        [[nodiscard]] uint32_t    sceneColorGeneration() const { return m_sceneColorGeneration; }
        [[nodiscard]] TVkImage&   sceneColor() { return m_sceneColor; }

    private:
        struct TFrameResources
        {
            TVkBuffer       m_lightGrid;
            TVkBuffer       m_lightIndices;
            TVkBuffer       m_counter;
            TVkBuffer       m_emitterBuf;
            TVkBuffer       m_particleSimUBO;
            VkDescriptorSet m_sceneSet        = VK_NULL_HANDLE;
            VkDescriptorSet m_cullSet         = VK_NULL_HANDLE;
            VkDescriptorSet m_spriteSet       = VK_NULL_HANDLE;
            VkDescriptorSet m_particleSimSet  = VK_NULL_HANDLE;
            VkDescriptorSet m_particleDrawSet = VK_NULL_HANDLE;
        };

        void createDepth();
        void createHdr();
        void createSceneColor();
        void createShadowResources();
        void createParticleResources();
        void createLayouts();
        void createFrameResources();
        void createPipelineLayouts();
        void createGraphicsPipelines();
        void destroyGraphicsPipelines();
        void createPipelines();
        void initPostStack();
        [[nodiscard]] TPostContext makePostContext();

        void recordShadowPasses( VkCommandBuffer p_cmd, const TFrameState& p_state, const TFrameResources& p_frame );
        void recordClusterCull( VkCommandBuffer p_cmd, const TFrameResources& p_frame );
        void recordForwardPass( VkCommandBuffer p_cmd, const TFrameState& p_state, const TFrameResources& p_frame );
        void recordSprites( VkCommandBuffer p_cmd, const TFrameState& p_state, const TFrameResources& p_frame );
        void recordParticleSim( VkCommandBuffer p_cmd, const TFrameResources& p_frame, uint32_t p_emitterCount );
        void recordParticlePass( VkCommandBuffer p_cmd, const TFrameState& p_state, const TFrameResources& p_frame );
        void recordPost( VkCommandBuffer p_cmd, uint32_t p_frameIndex, const TFrameState& p_state );

        VkDescriptorSet spriteTextureSet( const TVkImage* p_texture );

        [[nodiscard]] static TImgFormat imgFormatFromVk( VkFormat p_format );

        TVkGpu& m_gpu;

        VkExtent2D m_renderExtent{};

        TVkImage m_depth;
        TVkImage m_hdrA;
        TVkImage m_hdrB;
        TVkImage m_sceneColor;
        uint32_t m_sceneColorGeneration = 0;

        TVkImage                                 m_shadowMaps;
        std::array<VkImageView, k_maxShadowMaps> m_shadowLayerViews{};

        TVkBuffer m_particleBuf;
        TVkBuffer m_freeListBuf;
        TVkBuffer m_drawIndexBuf;
        TVkBuffer m_particleCounters;
        bool      m_particlesInitialized = false;
        uint32_t  m_particleFrameCounter = 0;

        VkDescriptorSetLayout m_sceneLayout        = VK_NULL_HANDLE;
        VkDescriptorSetLayout m_cullLayout         = VK_NULL_HANDLE;
        VkDescriptorSetLayout m_spriteSetLayout    = VK_NULL_HANDLE;
        VkDescriptorSetLayout m_spriteTexLayout    = VK_NULL_HANDLE;
        VkDescriptorSetLayout m_particleSimLayout  = VK_NULL_HANDLE;
        VkDescriptorSetLayout m_particleDrawLayout = VK_NULL_HANDLE;

        VkPipelineLayout m_shadowPipeLayout       = VK_NULL_HANDLE;
        VkPipelineLayout m_cullPipeLayout         = VK_NULL_HANDLE;
        VkPipelineLayout m_forwardPipeLayout      = VK_NULL_HANDLE;
        VkPipelineLayout m_spritePipeLayout       = VK_NULL_HANDLE;
        VkPipelineLayout m_particleSimPipeLayout  = VK_NULL_HANDLE;
        VkPipelineLayout m_particleDrawPipeLayout = VK_NULL_HANDLE;

        VkPipeline m_shadowPipeline        = VK_NULL_HANDLE;
        VkPipeline m_skinnedShadowPipeline = VK_NULL_HANDLE;
        VkPipeline m_cullPipeline          = VK_NULL_HANDLE;
        std::array<TMeshTechniquePipelines, static_cast<size_t>( TMeshTechniqueId::Count )> m_meshTechniques{};
        VkPipeline                                                                          m_spritePipeline      = VK_NULL_HANDLE;
        VkPipeline                                                                          m_particleSimPipeline = VK_NULL_HANDLE;
        VkPipeline                                                                          m_particlePipeline    = VK_NULL_HANDLE;

        std::vector<TFrameResources>                         m_frames;
        std::unordered_map<const TVkImage*, VkDescriptorSet> m_spriteTexSets;

        TPostStack m_post;
    };
}  // namespace Tomos
