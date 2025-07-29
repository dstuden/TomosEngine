#pragma once

#include <glm/glm.hpp>
#include <memory>
#include <vector>

#include "Texture.hh"

namespace Tomos
{
    class FrameBuffer
    {
    public:
        FrameBuffer( unsigned int p_width, unsigned int p_height, const std::vector<TextureFormat>& p_colorFormats = { TextureFormat::RGBA8 },
                     TextureFormat p_depthFormat = TextureFormat::Depth24 );
        ~FrameBuffer();

        void bind() const;
        void unbind() const;

        void resize( unsigned int p_width, unsigned int p_height );

        const std::vector<std::shared_ptr<Texture>>& getColorTextures() const { return m_colorTextures; }
        const std::shared_ptr<Texture>&              getColorTexture( unsigned int index = 0 ) const { return m_colorTextures.at( index ); }
        const std::shared_ptr<Texture>&              getDepthTexture() const { return m_depthTexture; }

        const glm::uvec2& getSize() const { return m_size; }

        bool         isValid() const { return m_fbo != 0; }
        unsigned int getId() const { return m_fbo; }

    private:
        unsigned int                          m_fbo = 0;
        std::vector<std::shared_ptr<Texture>> m_colorTextures;
        std::shared_ptr<Texture>              m_depthTexture;
        glm::uvec2                            m_size;
        std::vector<TextureFormat>            m_colorFormats;
        TextureFormat                         m_depthFormat;

        void initialize();
        void cleanup();
    };

    /*
     * LayerFrameBuffer is a specialized FrameBuffer that is used for rendering layers
     * It has only one color attachment
     * It is used for rendering the final output of a layer
     */
    class LayerFrameBuffer : public FrameBuffer
    {
    public:
        LayerFrameBuffer( unsigned int p_width, unsigned int p_height, TextureFormat p_colorFormat = TextureFormat::RGBA8,
                          TextureFormat p_depthFormat = TextureFormat::Depth24 )
            : FrameBuffer( p_width, p_height, { p_colorFormat }, p_depthFormat )
        {
        }
    };
}  // namespace Tomos
