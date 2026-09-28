#include "Tomos/util/image/TAnimatedTextureDecoder.hh"

#include <algorithm>
#include <cctype>
#include <mutex>
#include <unordered_map>

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}

#include "Tomos/util/logger/TLogger.hh"
#include "Tomos/util/path/TPath.hh"

namespace Tomos
{
    namespace
    {
        std::string lowerExt( const std::string& p_path )
        {
            const size_t      dot = p_path.find_last_of( '.' );
            if ( dot == std::string::npos ) return {};
            std::string ext = p_path.substr( dot );
            for ( char& c : ext ) c = static_cast<char>( std::tolower( static_cast<unsigned char>( c ) ) );
            return ext;
        }

        std::mutex                                                                       g_cacheMutex;
        std::unordered_map<std::string, std::weak_ptr<TAnimatedFrameCache>>              g_frameCaches;

        bool openFormat( const std::string& p_resolved, AVFormatContext** p_fmt )
        {
            if ( avformat_open_input( p_fmt, p_resolved.c_str(), nullptr, nullptr ) < 0 ) return false;
            if ( avformat_find_stream_info( *p_fmt, nullptr ) < 0 )
            {
                avformat_close_input( p_fmt );
                return false;
            }
            return true;
        }

        int findVideoStream( AVFormatContext* p_fmt )
        {
            for ( unsigned i = 0; i < p_fmt->nb_streams; ++i )
            {
                if ( p_fmt->streams[ i ]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO ) return static_cast<int>( i );
            }
            return -1;
        }

        float frameDurationSeconds( AVStream* p_stream, const AVFrame* p_frame, AVRational p_frameRate )
        {
            if ( p_frame->duration > 0 )
            {
                const double sec = p_frame->duration * av_q2d( p_stream->time_base );
                if ( sec > 0.0 ) return static_cast<float>( sec );
            }
            if ( p_frameRate.num > 0 && p_frameRate.den > 0 )
            {
                const double sec = av_q2d( av_inv_q( p_frameRate ) );
                if ( sec > 0.0 ) return static_cast<float>( sec );
            }
            return 1.0f / 24.0f;
        }

        bool scaleToRgba( SwsContext*& p_sws, AVFrame* p_src, int p_dstW, int p_dstH, std::vector<uint8_t>& p_out )
        {
            p_sws = sws_getCachedContext( p_sws, p_src->width, p_src->height, static_cast<AVPixelFormat>( p_src->format ), p_dstW, p_dstH,
                                          AV_PIX_FMT_RGBA, SWS_BILINEAR, nullptr, nullptr, nullptr );
            if ( p_sws == nullptr ) return false;

            p_out.resize( static_cast<size_t>( p_dstW ) * static_cast<size_t>( p_dstH ) * 4u );
            uint8_t* dstSlices[ 4 ]  = { p_out.data(), nullptr, nullptr, nullptr };
            int      dstStrides[ 4 ] = { p_dstW * 4, 0, 0, 0 };
            sws_scale( p_sws, p_src->data, p_src->linesize, 0, p_src->height, dstSlices, dstStrides );
            return true;
        }
    }  // namespace

    bool isStaticTexturePath( const std::string& p_path )
    {
        const std::string e = lowerExt( p_path );
        return e == ".png" || e == ".jpg" || e == ".jpeg" || e == ".bmp" || e == ".tga";
    }

    bool isCachedAnimatedPath( const std::string& p_path )
    {
        const std::string e = lowerExt( p_path );
        return e == ".gif" || e == ".webp";
    }

    bool isAnimatedTexturePath( const std::string& p_path )
    {
        const std::string e = lowerExt( p_path );
        return e == ".gif" || e == ".webp" || e == ".mp4" || e == ".webm" || e == ".mov" || e == ".mkv" || e == ".avi";
    }

    std::shared_ptr<TAnimatedFrameCache> decodeAnimatedFrameCache( const std::string& p_path )
    {
        const std::string resolved = TPath::resolveString( p_path );

        AVFormatContext* fmt = nullptr;
        if ( !openFormat( resolved, &fmt ) )
        {
            TLOG_WARN() << "[TAnimatedTextureDecoder] Failed to open: " << p_path;
            return {};
        }

        const int streamIdx = findVideoStream( fmt );
        if ( streamIdx < 0 )
        {
            TLOG_WARN() << "[TAnimatedTextureDecoder] No video stream: " << p_path;
            avformat_close_input( &fmt );
            return {};
        }

        AVStream*           stream = fmt->streams[ streamIdx ];
        const AVCodec*      codec  = avcodec_find_decoder( stream->codecpar->codec_id );
        AVCodecContext*     codecCtx = avcodec_alloc_context3( codec );
        if ( codec == nullptr || codecCtx == nullptr )
        {
            avformat_close_input( &fmt );
            return {};
        }
        if ( avcodec_parameters_to_context( codecCtx, stream->codecpar ) < 0 || avcodec_open2( codecCtx, codec, nullptr ) < 0 )
        {
            avcodec_free_context( &codecCtx );
            avformat_close_input( &fmt );
            return {};
        }

        const int canvasW = codecCtx->width > 0 ? codecCtx->width : stream->codecpar->width;
        const int canvasH = codecCtx->height > 0 ? codecCtx->height : stream->codecpar->height;
        if ( canvasW <= 0 || canvasH <= 0 )
        {
            avcodec_free_context( &codecCtx );
            avformat_close_input( &fmt );
            return {};
        }

        auto cache           = std::make_shared<TAnimatedFrameCache>();
        cache->m_width       = static_cast<uint32_t>( canvasW );
        cache->m_height      = static_cast<uint32_t>( canvasH );
        cache->m_totalDuration = 0.0f;

        AVPacket*   packet = av_packet_alloc();
        AVFrame*    frame  = av_frame_alloc();
        SwsContext* sws    = nullptr;
        AVRational  fr     = av_guess_frame_rate( fmt, stream, nullptr );

        while ( av_read_frame( fmt, packet ) >= 0 )
        {
            if ( packet->stream_index != streamIdx )
            {
                av_packet_unref( packet );
                continue;
            }
            if ( avcodec_send_packet( codecCtx, packet ) < 0 )
            {
                av_packet_unref( packet );
                continue;
            }
            av_packet_unref( packet );

            while ( avcodec_receive_frame( codecCtx, frame ) == 0 )
            {
                TAnimatedFrame out;
                out.m_duration = frameDurationSeconds( stream, frame, fr );
                if ( !scaleToRgba( sws, frame, canvasW, canvasH, out.m_rgba ) ) continue;
                cache->m_totalDuration += out.m_duration;
                cache->m_frames.push_back( std::move( out ) );
            }
        }

        // Flush decoder.
        avcodec_send_packet( codecCtx, nullptr );
        while ( avcodec_receive_frame( codecCtx, frame ) == 0 )
        {
            TAnimatedFrame out;
            out.m_duration = frameDurationSeconds( stream, frame, fr );
            if ( scaleToRgba( sws, frame, canvasW, canvasH, out.m_rgba ) )
            {
                cache->m_totalDuration += out.m_duration;
                cache->m_frames.push_back( std::move( out ) );
            }
        }

        if ( sws != nullptr ) sws_freeContext( sws );
        av_frame_free( &frame );
        av_packet_free( &packet );
        avcodec_free_context( &codecCtx );
        avformat_close_input( &fmt );

        if ( cache->m_frames.empty() )
        {
            TLOG_WARN() << "[TAnimatedTextureDecoder] No frames decoded: " << p_path;
            return {};
        }

        TLOG_INFO() << "[TAnimatedTextureDecoder] Cached " << cache->m_frames.size() << " frame(s) for '" << p_path << "' (" << cache->m_width << "x"
                    << cache->m_height << ")";
        return cache;
    }

    std::shared_ptr<TAnimatedFrameCache> getOrDecodeFrameCache( const std::string& p_path )
    {
        const std::string resolved = TPath::resolveString( p_path );
        {
            std::lock_guard lock( g_cacheMutex );
            const auto      it = g_frameCaches.find( resolved );
            if ( it != g_frameCaches.end() )
            {
                if ( auto existing = it->second.lock() ) return existing;
            }
        }

        auto decoded = decodeAnimatedFrameCache( p_path );
        if ( !decoded ) return {};

        std::lock_guard lock( g_cacheMutex );
        g_frameCaches[ resolved ] = decoded;
        return decoded;
    }

    struct TVideoDecoder::TImpl
    {
        AVFormatContext* m_fmt      = nullptr;
        AVCodecContext*  m_codecCtx = nullptr;
        SwsContext*      m_sws      = nullptr;
        AVPacket*        m_packet   = nullptr;
        AVFrame*         m_frame    = nullptr;
        int              m_streamIdx = -1;
        AVRational       m_frameRate{};
        bool             m_eof = false;
    };

    TVideoDecoder::TVideoDecoder() = default;

    TVideoDecoder::~TVideoDecoder() { close(); }

    TVideoDecoder::TVideoDecoder( TVideoDecoder&& p_other ) noexcept :
        m_impl( std::move( p_other.m_impl ) ), m_width( p_other.m_width ), m_height( p_other.m_height )
    {
        p_other.m_width  = 0;
        p_other.m_height = 0;
    }

    TVideoDecoder& TVideoDecoder::operator=( TVideoDecoder&& p_other ) noexcept
    {
        if ( this != &p_other )
        {
            close();
            m_impl             = std::move( p_other.m_impl );
            m_width            = p_other.m_width;
            m_height           = p_other.m_height;
            p_other.m_width  = 0;
            p_other.m_height = 0;
        }
        return *this;
    }

    bool TVideoDecoder::open( const std::string& p_path )
    {
        close();

        const std::string resolved = TPath::resolveString( p_path );
        auto              impl     = std::make_unique<TImpl>();

        if ( !openFormat( resolved, &impl->m_fmt ) )
        {
            TLOG_WARN() << "[TVideoDecoder] Failed to open: " << p_path;
            return false;
        }

        impl->m_streamIdx = findVideoStream( impl->m_fmt );
        if ( impl->m_streamIdx < 0 )
        {
            TLOG_WARN() << "[TVideoDecoder] No video stream: " << p_path;
            avformat_close_input( &impl->m_fmt );
            return false;
        }

        AVStream*      stream = impl->m_fmt->streams[ impl->m_streamIdx ];
        const AVCodec* codec  = avcodec_find_decoder( stream->codecpar->codec_id );
        impl->m_codecCtx      = avcodec_alloc_context3( codec );
        if ( codec == nullptr || impl->m_codecCtx == nullptr )
        {
            avformat_close_input( &impl->m_fmt );
            return false;
        }
        if ( avcodec_parameters_to_context( impl->m_codecCtx, stream->codecpar ) < 0 || avcodec_open2( impl->m_codecCtx, codec, nullptr ) < 0 )
        {
            avcodec_free_context( &impl->m_codecCtx );
            avformat_close_input( &impl->m_fmt );
            return false;
        }

        m_width  = static_cast<uint32_t>( std::max( 1, impl->m_codecCtx->width ) );
        m_height = static_cast<uint32_t>( std::max( 1, impl->m_codecCtx->height ) );
        if ( m_width == 0 || m_height == 0 )
        {
            m_width  = static_cast<uint32_t>( std::max( 1, stream->codecpar->width ) );
            m_height = static_cast<uint32_t>( std::max( 1, stream->codecpar->height ) );
        }

        impl->m_packet    = av_packet_alloc();
        impl->m_frame     = av_frame_alloc();
        impl->m_frameRate = av_guess_frame_rate( impl->m_fmt, stream, nullptr );
        impl->m_eof       = false;
        m_impl            = std::move( impl );
        return true;
    }

    void TVideoDecoder::close()
    {
        if ( !m_impl ) return;
        if ( m_impl->m_sws != nullptr ) sws_freeContext( m_impl->m_sws );
        if ( m_impl->m_frame != nullptr ) av_frame_free( &m_impl->m_frame );
        if ( m_impl->m_packet != nullptr ) av_packet_free( &m_impl->m_packet );
        if ( m_impl->m_codecCtx != nullptr ) avcodec_free_context( &m_impl->m_codecCtx );
        if ( m_impl->m_fmt != nullptr ) avformat_close_input( &m_impl->m_fmt );
        m_impl.reset();
        m_width  = 0;
        m_height = 0;
    }

    bool TVideoDecoder::rewind()
    {
        if ( !m_impl || m_impl->m_fmt == nullptr ) return false;
        if ( av_seek_frame( m_impl->m_fmt, m_impl->m_streamIdx, 0, AVSEEK_FLAG_BACKWARD ) < 0 ) return false;
        avcodec_flush_buffers( m_impl->m_codecCtx );
        m_impl->m_eof = false;
        return true;
    }

    bool TVideoDecoder::nextFrame( std::vector<uint8_t>& p_outRgba, float& p_outDuration )
    {
        if ( !m_impl || m_impl->m_eof ) return false;

        AVStream* stream = m_impl->m_fmt->streams[ m_impl->m_streamIdx ];

        for ( ;; )
        {
            // Drain decoder first.
            const int recv = avcodec_receive_frame( m_impl->m_codecCtx, m_impl->m_frame );
            if ( recv == 0 )
            {
                p_outDuration = frameDurationSeconds( stream, m_impl->m_frame, m_impl->m_frameRate );
                return scaleToRgba( m_impl->m_sws, m_impl->m_frame, static_cast<int>( m_width ), static_cast<int>( m_height ), p_outRgba );
            }
            if ( recv == AVERROR_EOF )
            {
                m_impl->m_eof = true;
                return false;
            }
            if ( recv != AVERROR( EAGAIN ) ) return false;

            if ( av_read_frame( m_impl->m_fmt, m_impl->m_packet ) < 0 )
            {
                avcodec_send_packet( m_impl->m_codecCtx, nullptr );
                continue;
            }

            if ( m_impl->m_packet->stream_index != m_impl->m_streamIdx )
            {
                av_packet_unref( m_impl->m_packet );
                continue;
            }

            const int send = avcodec_send_packet( m_impl->m_codecCtx, m_impl->m_packet );
            av_packet_unref( m_impl->m_packet );
            if ( send < 0 && send != AVERROR( EAGAIN ) ) return false;
        }
    }
}  // namespace Tomos
