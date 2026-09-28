#include "Tomos/gpu/vulkan/TVkMaterial.hh"

#include <array>
#include <stdexcept>

namespace Tomos
{
    TVkMaterial::TVkMaterial( VkDevice p_device, VkPhysicalDevice p_physDevice, VkDescriptorPool p_pool, VkDescriptorSetLayout p_layout,
                              const TVkMaterialDesc& p_desc ) :
        m_device( p_device ), m_uniformBuffer( p_device, p_physDevice, sizeof( TMaterialUBO ), TBufUsage::Uniform ), m_hasNormalMap( p_desc.m_hasNormalMap ),
        m_doubleSided( p_desc.m_doubleSided ), m_alphaMode( p_desc.m_alphaMode ), m_technique( p_desc.m_technique )
    {
        m_ubo.m_baseFactor     = p_desc.m_baseColorFactor;
        m_ubo.m_emissionFactor = p_desc.m_emissionFactor;
        m_ubo.m_metalFactor    = p_desc.m_metallicFactor;
        m_ubo.m_roughFactor    = p_desc.m_roughnessFactor;
        m_ubo.m_normalScale    = p_desc.m_normalScale;
        m_ubo.m_alphaCutoff    = p_desc.m_alphaCutoff;
        // MASK alpha-tests.  OPAQUE and BLEND skip discard; BLEND uses base.a
        // for framebuffer blending in the transparent forward pipelines.
        m_ubo.m_ignoreAlpha  = ( p_desc.m_alphaMode == TMatAlpha::Mask ) ? 0u : 1u;
        m_ubo.m_hasNormalMap = p_desc.m_hasNormalMap ? 1u : 0u;

        m_baseTexture     = p_desc.m_baseTexture;
        m_metRghTexture   = p_desc.m_metRghTexture;
        m_emissionTexture = p_desc.m_emissionTexture;
        m_normalTexture   = p_desc.m_normalTexture;

        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool     = p_pool;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts        = &p_layout;

        if ( vkAllocateDescriptorSets( m_device, &allocInfo, &m_descriptorSet ) != VK_SUCCESS )
            throw std::runtime_error( "[TVkMaterial] Failed to allocate descriptor set" );

        // Combined-image-samplers must never be VK_NULL_HANDLE.
        if ( p_desc.m_baseTexture == nullptr ) throw std::runtime_error( "[TVkMaterial] m_baseTexture is required (use gpu.defaultTexture())" );

        auto imgInfo = [ & ]( const TVkImage* p_tex ) -> VkDescriptorImageInfo
        {
            const TVkImage*       tex = ( p_tex != nullptr ) ? p_tex : p_desc.m_baseTexture;
            VkDescriptorImageInfo info{};
            info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            info.imageView   = tex->view();
            info.sampler     = tex->sampler();
            return info;
        };

        VkDescriptorBufferInfo bufInfo{};
        bufInfo.buffer = m_uniformBuffer.handle();
        bufInfo.offset = 0;
        bufInfo.range  = sizeof( TMaterialUBO );

        VkDescriptorImageInfo baseInfo = imgInfo( p_desc.m_baseTexture );
        VkDescriptorImageInfo mrInfo   = imgInfo( p_desc.m_metRghTexture );
        VkDescriptorImageInfo emsInfo  = imgInfo( p_desc.m_emissionTexture );
        VkDescriptorImageInfo normInfo = imgInfo( p_desc.m_normalTexture );

        std::array<VkWriteDescriptorSet, 5> writes{};

        writes[ 0 ] = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, m_descriptorSet, 0,      0, 1,
                        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,      nullptr, &bufInfo,        nullptr };
        writes[ 1 ] = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,    nullptr,   m_descriptorSet, 1,      0, 1,
                        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &baseInfo, nullptr,         nullptr };
        writes[ 2 ] = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,    nullptr, m_descriptorSet, 2,      0, 1,
                        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &mrInfo, nullptr,         nullptr };
        writes[ 3 ] = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,    nullptr,  m_descriptorSet, 3,      0, 1,
                        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &emsInfo, nullptr,         nullptr };
        writes[ 4 ] = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,    nullptr,   m_descriptorSet, 4,      0, 1,
                        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &normInfo, nullptr,         nullptr };

        vkUpdateDescriptorSets( m_device, static_cast<uint32_t>( writes.size() ), writes.data(), 0, nullptr );

        push();
    }

    void TVkMaterial::push() { m_uniformBuffer.upload( &m_ubo, 0, sizeof( TMaterialUBO ) ); }
}  // namespace Tomos
