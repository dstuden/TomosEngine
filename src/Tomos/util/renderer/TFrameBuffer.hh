#pragma once

#include <glm/glm.hpp>
#include <memory>
#include <vector>

#include "TTexture.hh"

namespace Tomos
{
    class TFrameBuffer
    {
    public:
        TFrameBuffer( unsigned int p_width, unsigned int p_height, const std::vector<TTextureFormat>& p_colorFormats = { TTextureFormat::RGBA8 },
                     TTextureFormat p_depthFormat = TTextureFormat::Depth24 );
        ~TFrameBuffer();

        void bind() const;
        void unbind() const;

        void resize( unsigned int p_width, unsigned int p_height );

        const std::vector<std::shared_ptr<TTexture>>& getColorTextures() const { return m_colorTextures; }
        const std::shared_ptr<TTexture>&              getColorTexture( unsigned int index = 0 ) const { return m_colorTextures.at( index ); }
        const std::shared_ptr<TTexture>&              getDepthTexture() const { return m_depthTexture; }

        const glm::uvec2& getSize() const { return m_size; }

        bool         isValid() const { return m_fbo != 0; }
        unsigned int getId() const { return m_fbo; }

    private:
        unsigned int                          m_fbo = 0;
        std::vector<std::shared_ptr<TTexture>> m_colorTextures;
        std::shared_ptr<TTexture>              m_depthTexture;
        glm::uvec2                            m_size;
        std::vector<TTextureFormat>            m_colorFormats;
        TTextureFormat                         m_depthFormat;

        void initialize();
        void cleanup();
    };

    /*
     * LayerFrameBuffer is a specialized FrameBuffer that is used for rendering layers
     * It has only one color attachment
     * It is used for rendering the final output of a layer
     */
    class LayerFrameBuffer : public TFrameBuffer
    {
    public:
        LayerFrameBuffer( unsigned int p_width, unsigned int p_height, TTextureFormat p_colorFormat = TTextureFormat::RGBA8,
                          TTextureFormat p_depthFormat = TTextureFormat::Depth24 )
            : TFrameBuffer( p_width, p_height, { p_colorFormat }, p_depthFormat )
        {
        }
    };
}  // namespace Tomos
