#include "Tomos/util/image/TAnimatedTexture.hh"

#include "Tomos/gpu/TGpuEnums.hh"
#include "Tomos/gpu/vulkan/TVkGpu.hh"
#include "Tomos/util/logger/TLogger.hh"

namespace Tomos
{
    TAnimatedTexture::TAnimatedTexture() = default;
    TAnimatedTexture::~TAnimatedTexture()  = default;

    bool TAnimatedTexture::create( TVkGpu& p_gpu, const std::string& p_path, const TBagAnimatedTextureRef& p_opts )
    {
        m_path = p_path;
        applyOpts( p_opts );
        m_frameIndex   = 0;
        m_frameTime    = 0.0f;
        m_ended        = false;
        m_dirty        = true;
        m_useVideo     = false;
        m_cache.reset();
        m_video.close();
        m_image = TVkImage{};

        uint32_t width  = 0;
        uint32_t height = 0;
        const uint8_t* firstPixels = nullptr;

        if ( isCachedAnimatedPath( p_path ) )
        {
            m_cache = getOrDecodeFrameCache( p_path );
            if ( !m_cache || m_cache->m_frames.empty() ) return false;
            width          = m_cache->m_width;
            height         = m_cache->m_height;
            firstPixels    = m_cache->m_frames.front().m_rgba.data();
            m_holdDuration = m_cache->m_frames.front().m_duration;
            m_useVideo     = false;
        }
        else
        {
            if ( !m_video.open( p_path ) ) return false;
            width      = m_video.width();
            height     = m_video.height();
            m_useVideo = true;
            float dur  = m_holdDuration;
            if ( !m_video.nextFrame( m_scratch, dur ) )
            {
                TLOG_WARN() << "[TAnimatedTexture] Failed to decode first frame: " << p_path;
                m_video.close();
                return false;
            }
            m_holdDuration = dur;
            firstPixels    = m_scratch.data();
        }

        TVkImageDesc desc{};
        desc.m_width     = width;
        desc.m_height    = height;
        desc.m_mipLevels = 1;
        desc.m_format    = TImgFormat::RGBA8Unorm;
        desc.m_usage     = TImgUsage::CopySrc | TImgUsage::CopyDst | TImgUsage::Sampled;
        desc.m_sampled   = true;
        desc.m_addr      = TTexAddr::Clamp;

        m_image = TVkImage( p_gpu.device(), p_gpu.physDevice(), desc );
        p_gpu.uploadImage( m_image, firstPixels, width, height );
        m_dirty = false;

        TLOG_INFO() << "[TAnimatedTexture] Created '" << p_path << "' (" << width << "x" << height << ")"
                    << ( m_useVideo ? " [video]" : " [cached]" );
        return true;
    }

    void TAnimatedTexture::uploadFrame( TVkGpu& p_gpu, const uint8_t* p_rgba )
    {
        if ( !m_image.valid() || p_rgba == nullptr ) return;
        p_gpu.updateImage( m_image, p_rgba, m_image.width(), m_image.height() );
        m_dirty = false;
    }

    void TAnimatedTexture::tick( TVkGpu& p_gpu, float p_dt )
    {
        if ( !m_image.valid() || !m_playing || m_ended ) return;
        if ( p_dt <= 0.0f && !m_dirty ) return;

        const float speed = m_speed > 0.0f ? m_speed : 0.0f;
        m_frameTime += p_dt * speed;

        if ( !m_useVideo )
        {
            if ( !m_cache || m_cache->m_frames.empty() ) return;

            while ( m_frameTime >= m_holdDuration )
            {
                m_frameTime -= m_holdDuration;
                const uint32_t next = m_frameIndex + 1;
                if ( next >= m_cache->m_frames.size() )
                {
                    if ( m_looping )
                    {
                        m_frameIndex = 0;
                    }
                    else
                    {
                        m_frameIndex = static_cast<uint32_t>( m_cache->m_frames.size() ) - 1;
                        m_ended      = true;
                        m_frameTime  = 0.0f;
                        break;
                    }
                }
                else
                {
                    m_frameIndex = next;
                }
                m_holdDuration = m_cache->m_frames[ m_frameIndex ].m_duration;
                m_dirty        = true;
            }

            if ( m_dirty ) uploadFrame( p_gpu, m_cache->m_frames[ m_frameIndex ].m_rgba.data() );
            return;
        }

        // Video path: advance by holding each decoded frame for its duration.
        while ( m_frameTime >= m_holdDuration )
        {
            m_frameTime -= m_holdDuration;
            float dur = m_holdDuration;
            if ( !m_video.nextFrame( m_scratch, dur ) )
            {
                if ( m_looping && m_video.rewind() && m_video.nextFrame( m_scratch, dur ) )
                {
                    m_holdDuration = dur;
                    m_dirty        = true;
                }
                else
                {
                    m_ended     = true;
                    m_frameTime = 0.0f;
                    break;
                }
            }
            else
            {
                m_holdDuration = dur;
                m_dirty        = true;
            }
        }

        if ( m_dirty ) uploadFrame( p_gpu, m_scratch.data() );
    }
}  // namespace Tomos
