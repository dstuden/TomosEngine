#pragma once

#include <glm/glm.hpp>
#include <vulkan/vulkan.h>

#include "Tomos/gpu/TGpuEnums.hh"
#include "Tomos/gpu/vulkan/TMeshTechnique.hh"
#include "Tomos/gpu/vulkan/TVkBuffer.hh"
#include "Tomos/gpu/vulkan/TVkImage.hh"

namespace Tomos
{
    struct TVkMaterialDesc
    {
        glm::vec4        m_baseColorFactor = glm::vec4( 1.0f );
        glm::vec3        m_emissionFactor  = glm::vec3( 0.0f );
        float            m_metallicFactor  = 0.0f;
        float            m_roughnessFactor = 1.0f;
        float            m_normalScale     = 1.0f;
        float            m_alphaCutoff     = 0.5f;
        TMatAlpha        m_alphaMode       = TMatAlpha::Opaque;
        TMeshTechniqueId m_technique       = TMeshTechniqueId::Forward;
        bool             m_doubleSided     = false;
        bool             m_hasNormalMap    = false;

        // Textures borrowed — material does not own them.
        const TVkImage* m_baseTexture     = nullptr;
        const TVkImage* m_metRghTexture   = nullptr;
        const TVkImage* m_emissionTexture = nullptr;
        const TVkImage* m_normalTexture   = nullptr;
    };

    class TVkMaterial
    {
    public:
        TVkMaterial() = default;
        TVkMaterial( VkDevice p_device, VkPhysicalDevice p_physDevice, VkDescriptorPool p_pool, VkDescriptorSetLayout p_layout, const TVkMaterialDesc& p_desc );

        TVkMaterial( const TVkMaterial& )            = delete;
        TVkMaterial& operator=( const TVkMaterial& ) = delete;

        void push();

        void setBaseColorFactor( const glm::vec4& p_v )
        {
            m_ubo.m_baseFactor = p_v;
            push();
        }
        void setEmissionFactor( const glm::vec3& p_v )
        {
            m_ubo.m_emissionFactor = p_v;
            push();
        }
        void setMetallicFactor( float p_v )
        {
            m_ubo.m_metalFactor = p_v;
            push();
        }
        void setRoughnessFactor( float p_v )
        {
            m_ubo.m_roughFactor = p_v;
            push();
        }
        void setNormalScale( float p_v )
        {
            m_ubo.m_normalScale = p_v;
            push();
        }
        void setAlphaCutoff( float p_v )
        {
            m_ubo.m_alphaCutoff = p_v;
            push();
        }

        [[nodiscard]] const glm::vec4& baseColorFactor() const { return m_ubo.m_baseFactor; }
        [[nodiscard]] const glm::vec3& emissionFactor() const { return m_ubo.m_emissionFactor; }
        [[nodiscard]] float            metallicFactor() const { return m_ubo.m_metalFactor; }
        [[nodiscard]] float            roughnessFactor() const { return m_ubo.m_roughFactor; }
        [[nodiscard]] float            normalScale() const { return m_ubo.m_normalScale; }
        [[nodiscard]] float            alphaCutoff() const { return m_ubo.m_alphaCutoff; }

        [[nodiscard]] VkDescriptorSet  descriptorSet() const { return m_descriptorSet; }
        [[nodiscard]] bool             hasNormalMap() const { return m_hasNormalMap; }
        [[nodiscard]] bool             doubleSided() const { return m_doubleSided; }
        [[nodiscard]] TMatAlpha        alphaMode() const { return m_alphaMode; }
        [[nodiscard]] TMeshTechniqueId technique() const { return m_technique; }

    private:
        // Matches Material in shaders (std140).
        struct alignas( 16 ) TMaterialUBO
        {
            glm::vec4 m_baseFactor;
            glm::vec3 m_emissionFactor;
            float     m_metalFactor;
            float     m_roughFactor;
            float     m_normalScale;
            float     m_alphaCutoff;
            uint32_t  m_ignoreAlpha;  // 1 = OPAQUE/BLEND (skip alpha-test); 0 = MASK
            uint32_t  m_hasNormalMap;  // 1 = sample tNormal into TBN
            uint32_t  m_pad0 = 0;
            uint32_t  m_pad1 = 0;
        };

        VkDevice         m_device        = VK_NULL_HANDLE;
        VkDescriptorSet  m_descriptorSet = VK_NULL_HANDLE;
        TVkBuffer        m_uniformBuffer;
        TMaterialUBO     m_ubo{};
        bool             m_hasNormalMap = false;
        bool             m_doubleSided  = false;
        TMatAlpha        m_alphaMode    = TMatAlpha::Opaque;
        TMeshTechniqueId m_technique    = TMeshTechniqueId::Forward;
    };
}  // namespace Tomos
