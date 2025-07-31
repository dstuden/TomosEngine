//
// Created by dstuden on 4/2/25.
//
#include "TMaterial.hh"
#include "Tomos/util/logger/TLogger.hh"

namespace Tomos
{
    // Static member initialization
    std::shared_ptr<TTexture> TMaterial::g_defaultWhite   = nullptr;
    std::shared_ptr<TTexture> TMaterial::g_defaultBlack   = nullptr;
    std::shared_ptr<TTexture> TMaterial::g_defaultNormal  = nullptr;
    std::shared_ptr<TSampler> TMaterial::g_defaultSampler = nullptr;

    TMaterial::TMaterial(
            const std::shared_ptr<TShader>&  p_shader,
            const glm::vec4&                p_baseFactor,
            const glm::vec3&                p_emissionFactor,
            float                           p_metalFactor,
            float                           p_roughFactor,
            float                           p_normalScale,
            float                           p_alphaCutoff,
            AlphaMode                       p_alphaMode,
            const std::shared_ptr<TTexture>& p_baseTexture,
            const std::shared_ptr<TTexture>& p_metalRoughTexture,
            const std::shared_ptr<TTexture>& p_emissionTexture,
            const std::shared_ptr<TTexture>& p_normalTexture,
            const std::shared_ptr<TSampler>& p_sampler,
            const std::string&              p_label
            ) :
        m_shader( p_shader ),
        m_label( p_label ),
        m_baseFactor( p_baseFactor ),
        m_emissionFactor( p_emissionFactor ),
        m_metalFactor( p_metalFactor ),
        m_roughFactor( p_roughFactor ),
        m_normalScale( p_normalScale ),
        m_alphaCutoff( p_alphaCutoff ),
        m_alphaMode( p_alphaMode ),
        m_baseTexture( p_baseTexture ? p_baseTexture : getDefaultWhite() ),
        m_metalRoughTexture( p_metalRoughTexture ? p_metalRoughTexture : getDefaultWhite() ),
        m_emissionTexture( p_emissionTexture ? p_emissionTexture : getDefaultBlack() ),
        m_normalTexture( p_normalTexture ? p_normalTexture : getDefaultNormal() ),
        m_sampler( p_sampler ? p_sampler : getDefaultSampler() )
    {
        TLOG_DEBUG() << "Material created: " << m_label;
    }

    void TMaterial::bind() const
    {
        if ( !m_shader )
        {
            TLOG_WARN() << "Material has no shader set: " << m_label;
            return;
        }

        m_shader->bind();

        // Set material properties
        m_shader->setVec4( "uBaseFactor", m_baseFactor );
        m_shader->setVec3( "uEmissionFactor", m_emissionFactor );
        m_shader->setFloat( "uMetalFactor", m_metalFactor );
        m_shader->setFloat( "uRoughFactor", m_roughFactor );
        m_shader->setFloat( "uNormalScale", m_normalScale );
        m_shader->setFloat( "uAlphaCutoff", m_alphaCutoff );
        m_shader->setInt( "uIgnoreAlpha", m_alphaMode == AlphaMode::OPAQUE ? 1 : 0 );

        // Texture unit assignments
        enum TextureUnits
        {
            BASE = 0,
            METALROUGH = 1,
            EMISSION = 2,
            NORMAL = 3
        };

        // Bind textures to their units
        m_baseTexture->bind( BASE );
        m_shader->setInt( "uBaseTexture", BASE );

        m_metalRoughTexture->bind( METALROUGH );
        m_shader->setInt( "uMetalRoughTexture", METALROUGH );

        m_emissionTexture->bind( EMISSION );
        m_shader->setInt( "uEmissionTexture", EMISSION );

        if ( m_normalTexture )
        {
            m_normalTexture->bind( NORMAL );
        }
        else
        {
            getDefaultNormal()->bind( NORMAL );
        }
        m_shader->setInt( "uNormalTexture", NORMAL );

        // Bind sampler to all units (if using separate samplers)
        if ( m_sampler )
        {
            m_sampler->bind( BASE );
            m_sampler->bind( METALROUGH );
            m_sampler->bind( EMISSION );
            m_sampler->bind( NORMAL );
        }
    }

    // Property setters (update both cached value and shader uniform)
    void TMaterial::setBaseFactor( const glm::vec4& p_factor )
    {
        m_baseFactor = p_factor;
        if ( m_shader )
        {
            m_shader->bind();
            m_shader->setVec4( "uBaseFactor", m_baseFactor );
        }
    }

    void TMaterial::setEmission( const glm::vec3& p_emission )
    {
        m_emissionFactor = p_emission;
        if ( m_shader )
        {
            m_shader->bind();
            m_shader->setVec3( "uEmissionFactor", m_emissionFactor );
        }
    }

    void TMaterial::setMetallicRoughness( float p_metallic, float p_roughness )
    {
        m_metalFactor = p_metallic;
        m_roughFactor = p_roughness;
        if ( m_shader )
        {
            m_shader->bind();
            m_shader->setFloat( "uMetalFactor", m_metalFactor );
            m_shader->setFloat( "uRoughFactor", m_roughFactor );
        }
    }

    void TMaterial::setNormalScale( float p_scale )
    {
        m_normalScale = p_scale;
        if ( m_shader )
        {
            m_shader->bind();
            m_shader->setFloat( "uNormalScale", m_normalScale );
        }
    }

    void TMaterial::setAlphaCutoff( float p_cutoff )
    {
        m_alphaCutoff = p_cutoff;
        if ( m_shader )
        {
            m_shader->bind();
            m_shader->setFloat( "uAlphaCutoff", m_alphaCutoff );
        }
    }

    // Texture setters
    void TMaterial::setBaseTexture( const std::shared_ptr<TTexture>& p_texture )
    {
        m_baseTexture = p_texture ? p_texture : getDefaultWhite();
    }

    void TMaterial::setMetalRoughTexture( const std::shared_ptr<TTexture>& p_texture )
    {
        m_metalRoughTexture = p_texture ? p_texture : getDefaultWhite();
    }

    void TMaterial::setEmissionTexture( const std::shared_ptr<TTexture>& p_texture )
    {
        m_emissionTexture = p_texture ? p_texture : getDefaultBlack();
    }

    void TMaterial::setNormalTexture( const std::shared_ptr<TTexture>& p_texture )
    {
        m_normalTexture = p_texture ? p_texture : getDefaultNormal();
    }

    // Default texture getters
    std::shared_ptr<TTexture> TMaterial::getDefaultWhite()
    {
        if ( !g_defaultWhite )
        {
            g_defaultWhite = TTexture::createWhiteTexture();
        }
        return g_defaultWhite;
    }

    std::shared_ptr<TTexture> TMaterial::getDefaultBlack()
    {
        if ( !g_defaultBlack )
        {
            g_defaultBlack = TTexture::createBlackTexture();
        }
        return g_defaultBlack;
    }

    std::shared_ptr<TTexture> TMaterial::getDefaultNormal()
    {
        if ( !g_defaultNormal )
        {
            g_defaultNormal = TTexture::createDefaultNormalMap();
        }
        return g_defaultNormal;
    }

    std::shared_ptr<TSampler> TMaterial::getDefaultSampler()
    {
        if ( !g_defaultSampler )
        {
            g_defaultSampler = TSampler::createLinearRepeat();
        }
        return g_defaultSampler;
    }
} // namespace Tomos
