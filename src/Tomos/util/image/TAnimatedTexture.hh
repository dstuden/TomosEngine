#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "Tomos/gpu/vulkan/TVkImage.hh"
#include "Tomos/systems/asset/TAssetHandles.hh"
#include "Tomos/util/image/TAnimatedTextureDecoder.hh"

namespace Tomos
{
    class TVkGpu;

    // Playback instance: owns a stable TVkImage (mipLevels=1) updated each tick.
    class TAnimatedTexture
    {
    public:
        TAnimatedTexture();
        ~TAnimatedTexture();

        TAnimatedTexture( const TAnimatedTexture& )            = delete;
        TAnimatedTexture& operator=( const TAnimatedTexture& ) = delete;

        // Create GPU image + decoder/cache. Returns false on failure (image invalid).
        [[nodiscard]] bool create( TVkGpu& p_gpu, const std::string& p_path, const TBagAnimatedTextureRef& p_opts );

        void tick( TVkGpu& p_gpu, float p_dt );

        [[nodiscard]] TVkImage*       image() { return m_image.valid() ? &m_image : nullptr; }
        [[nodiscard]] const TVkImage* image() const { return m_image.valid() ? &m_image : nullptr; }
        [[nodiscard]] bool            valid() const { return m_image.valid(); }
        [[nodiscard]] const std::string& path() const { return m_path; }

        bool  m_looping = true;
        bool  m_playing = true;
        float m_speed   = 1.0f;

        void applyOpts( const TBagAnimatedTextureRef& p_opts )
        {
            m_looping = p_opts.m_looping;
            m_playing = p_opts.m_playing;
            m_speed   = p_opts.m_speed;
        }

    private:
        void uploadFrame( TVkGpu& p_gpu, const uint8_t* p_rgba );

        std::string                         m_path;
        TVkImage                            m_image;
        std::shared_ptr<TAnimatedFrameCache> m_cache;
        TVideoDecoder                       m_video;
        std::vector<uint8_t>                m_scratch;

        uint32_t m_frameIndex   = 0;
        float    m_frameTime    = 0.0f;
        float    m_holdDuration = 1.0f / 24.0f;
        bool     m_dirty        = true;
        bool     m_useVideo     = false;
        bool     m_ended        = false;
    };
}  // namespace Tomos
