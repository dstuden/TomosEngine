#pragma once

#include <glm/glm.hpp>
#include <memory>

#include "Tomos/util/renderer/TSampler.hh"
#include "Tomos/util/renderer/TShader.hh"
#include "Tomos/util/renderer/TTexture.hh"

namespace Tomos
{
    enum class AlphaMode
    {
        OPAQUE,
        MASK,
        BLEND
    };

    class TMaterial
    {
    public:
        TMaterial(
                const std::shared_ptr<TShader>&  p_shader,
                const glm::vec4&                p_baseFactor        = glm::vec4( 1.0f ),
                const glm::vec3&                p_emissionFactor    = glm::vec3( 0.0f ),
                float                           p_metalFactor       = 0.5f,
                float                           p_roughFactor       = 0.5f,
                float                           p_normalScale       = 1.0f,
                float                           p_alphaCutoff       = 0.5f,
                AlphaMode                       p_alphaMode         = AlphaMode::OPAQUE,
                const std::shared_ptr<TTexture>& p_baseTexture       = nullptr,
                const std::shared_ptr<TTexture>& p_metalRoughTexture = nullptr,
                const std::shared_ptr<TTexture>& p_emissionTexture   = nullptr,
                const std::shared_ptr<TTexture>& p_normalTexture     = nullptr,
                const std::shared_ptr<TSampler>& p_sampler           = nullptr,
                const std::string&              p_label             = ""
                );

        void bind() const;

        // Property setters
        void setBaseFactor( const glm::vec4& p_factor );
        void setEmission( const glm::vec3& p_emission );
        void setMetallicRoughness( float p_metallic, float p_roughness );
        void setNormalScale( float p_scale );
        void setAlphaCutoff( float p_cutoff );
        void setAlphaMode( AlphaMode p_mode ) { m_alphaMode = p_mode; }

        // Texture setters
        void setBaseTexture( const std::shared_ptr<TTexture>& p_texture );
        void setMetalRoughTexture( const std::shared_ptr<TTexture>& p_texture );
        void setEmissionTexture( const std::shared_ptr<TTexture>& p_texture );
        void setNormalTexture( const std::shared_ptr<TTexture>& p_texture );

        void setLabel( const std::string& p_label ) { m_label = p_label; }
        void setAtlasCoordinates( const glm::vec4& p_coordinates ) { m_atlasCoordinates = p_coordinates; }

        const std::shared_ptr<TShader>& getShader() const { return m_shader; }
        const std::string&             getLabel() const { return m_label; }
        const glm::vec4&               getAtlasCoordinates() const { return m_atlasCoordinates; }

        static std::shared_ptr<TTexture> getDefaultWhite();
        static std::shared_ptr<TTexture> getDefaultBlack();
        static std::shared_ptr<TTexture> getDefaultNormal();
        static std::shared_ptr<TSampler> getDefaultSampler();

        const std::shared_ptr<TTexture>& getBaseTexture() const { return m_baseTexture; }
        const std::shared_ptr<TTexture>& getMetalRoughTexture() const { return m_metalRoughTexture; }
        const std::shared_ptr<TTexture>& getEmissionTexture() const { return m_emissionTexture; }
        const std::shared_ptr<TTexture>& getNormalTexture() const { return m_normalTexture; }
        const std::shared_ptr<TSampler>& getSampler() const { return m_sampler; }

    private:
        std::shared_ptr<TShader> m_shader;
        std::string             m_label;

        // Material properties
        glm::vec4 m_baseFactor;
        glm::vec3 m_emissionFactor;
        float     m_metalFactor;
        float     m_roughFactor;
        float     m_normalScale;
        float     m_alphaCutoff;
        AlphaMode m_alphaMode;
        glm::vec4 m_atlasCoordinates = glm::vec4( 0.0f, 0.0f, 1.0f, 1.0f ); // Default UV coordinates

        // Textures
        std::shared_ptr<TTexture> m_baseTexture;
        std::shared_ptr<TTexture> m_metalRoughTexture;
        std::shared_ptr<TTexture> m_emissionTexture;
        std::shared_ptr<TTexture> m_normalTexture;
        std::shared_ptr<TSampler> m_sampler;

        // Default textures
        static std::shared_ptr<TTexture> g_defaultWhite;
        static std::shared_ptr<TTexture> g_defaultBlack;
        static std::shared_ptr<TTexture> g_defaultNormal;
        static std::shared_ptr<TSampler> g_defaultSampler;
    };
} // namespace Tomos
