#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace Tomos
{
    struct TAnimatedFrame
    {
        std::vector<uint8_t> m_rgba;
        float                m_duration = 1.0f / 24.0f;
    };

    // Shared CPU frame cache for GIF / animated WebP (decoded once per path).
    struct TAnimatedFrameCache
    {
        uint32_t                    m_width  = 0;
        uint32_t                    m_height = 0;
        std::vector<TAnimatedFrame> m_frames;
        float                       m_totalDuration = 0.0f;
    };

    [[nodiscard]] bool isAnimatedTexturePath( const std::string& p_path );
    [[nodiscard]] bool isStaticTexturePath( const std::string& p_path );
    [[nodiscard]] bool isCachedAnimatedPath( const std::string& p_path );

    // Decode all frames (GIF / animated WebP). Empty shared_ptr on failure.
    [[nodiscard]] std::shared_ptr<TAnimatedFrameCache> decodeAnimatedFrameCache( const std::string& p_path );

    // Shared weak-cache by resolved path.
    [[nodiscard]] std::shared_ptr<TAnimatedFrameCache> getOrDecodeFrameCache( const std::string& p_path );

    // Streaming video decode (one decoder per playback instance).
    class TVideoDecoder
    {
    public:
        TVideoDecoder();
        ~TVideoDecoder();

        TVideoDecoder( const TVideoDecoder& )            = delete;
        TVideoDecoder& operator=( const TVideoDecoder& ) = delete;
        TVideoDecoder( TVideoDecoder&& p_other ) noexcept;
        TVideoDecoder& operator=( TVideoDecoder&& p_other ) noexcept;

        [[nodiscard]] bool open( const std::string& p_path );
        void               close();

        // Decode next frame into p_outRgba (size = width*height*4). Returns false at EOF.
        [[nodiscard]] bool nextFrame( std::vector<uint8_t>& p_outRgba, float& p_outDuration );

        // Seek to start for looping.
        [[nodiscard]] bool rewind();

        [[nodiscard]] uint32_t width() const { return m_width; }
        [[nodiscard]] uint32_t height() const { return m_height; }
        [[nodiscard]] bool     valid() const { return m_impl != nullptr; }

    private:
        struct TImpl;
        std::unique_ptr<TImpl> m_impl;
        uint32_t               m_width  = 0;
        uint32_t               m_height = 0;
    };
}  // namespace Tomos
