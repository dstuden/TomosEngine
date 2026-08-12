#include "Tomos/core/scene/TSceneResourceBag.hh"

#include "Tomos/gpu/TGpuEnums.hh"
#include "Tomos/gpu/vulkan/TVkGpu.hh"
#include "Tomos/util/image/TImageLoad.hh"
#include "Tomos/util/logger/TLogger.hh"

namespace Tomos
{
    TAudioClip* TSceneResourceBag::addClip( std::unique_ptr<TAudioClip> p_clip )
    {
        if ( p_clip == nullptr ) return nullptr;
        TAudioClip* raw    = p_clip.get();
        m_clipPaths[ raw ] = raw->m_path;
        if ( !raw->m_path.empty() ) m_clipsByPath[ raw->m_path ] = raw;
        m_clips.push_back( std::move( p_clip ) );
        return raw;
    }

    TAudioClip* TSceneResourceBag::getOrCreateClip( const std::string& p_path, const std::string& p_name )
    {
        if ( auto* existing = findClipByPath( p_path ) ) return existing;
        return addClip( std::make_unique<TAudioClip>( p_path, p_name.empty() ? p_path : p_name ) );
    }

    TAudioClip* TSceneResourceBag::findClipByPath( const std::string& p_path ) const
    {
        const auto it = m_clipsByPath.find( p_path );
        return it != m_clipsByPath.end() ? it->second : nullptr;
    }

    const std::string* TSceneResourceBag::findClipPath( const TAudioClip* p_clip ) const
    {
        const auto it = m_clipPaths.find( p_clip );
        return it != m_clipPaths.end() ? &it->second : nullptr;
    }

    TVkImage* TSceneResourceBag::addImage( const std::string& p_path, std::unique_ptr<TVkImage> p_image )
    {
        if ( p_image == nullptr ) return nullptr;
        TVkImage* raw            = p_image.get();
        m_imagesByPath[ p_path ] = raw;
        m_imagePaths[ raw ]      = p_path;
        m_failedImagePaths.erase( p_path );
        m_images.push_back( std::move( p_image ) );
        return raw;
    }

    TVkImage* TSceneResourceBag::findImage( const std::string& p_path ) const
    {
        const auto it = m_imagesByPath.find( p_path );
        return it != m_imagesByPath.end() ? it->second : nullptr;
    }

    const std::string* TSceneResourceBag::findImagePath( const TVkImage* p_image ) const
    {
        const auto it = m_imagePaths.find( p_image );
        return it != m_imagePaths.end() ? &it->second : nullptr;
    }

    TVkImage* TSceneResourceBag::loadImage( TVkGpu& p_gpu, const std::string& p_path )
    {
        if ( auto* existing = findImage( p_path ) ) return existing;

        if ( m_failedImagePaths.contains( p_path ) ) return const_cast<TVkImage*>( &p_gpu.missingTexture() );

        TRgbaPixels filePx = loadRgbaFile( p_path );
        if ( !filePx )
        {
            TLOG_WARN() << "[TSceneResourceBag] Failed to load image: " << p_path << " — using missing texture";
            // Device sentinel — not bag-owned. Remember the path so we don't re-warn,
            // but never insert missingTexture into m_imagesByPath / m_imagePaths.
            m_failedImagePaths.insert( p_path );
            return const_cast<TVkImage*>( &p_gpu.missingTexture() );
        }

        TVkImageDesc desc{};
        desc.m_width     = static_cast<uint32_t>( filePx.m_width );
        desc.m_height    = static_cast<uint32_t>( filePx.m_height );
        desc.m_mipLevels = TVkImage::calcMipLevels( desc.m_width, desc.m_height );
        desc.m_format    = TImgFormat::RGBA8Unorm;
        desc.m_usage     = TImgUsage::CopySrc | TImgUsage::CopyDst | TImgUsage::Sampled;
        desc.m_sampled   = true;
        desc.m_addr      = TTexAddr::Clamp;

        auto image = std::make_unique<TVkImage>( p_gpu.device(), p_gpu.physDevice(), desc );
        p_gpu.uploadImage( *image, filePx.pixels(), desc.m_width, desc.m_height );

        TLOG_INFO() << "[TSceneResourceBag] Loaded image " << p_path << " (" << filePx.m_width << "x" << filePx.m_height << ")";
        return addImage( p_path, std::move( image ) );
    }

    void TSceneResourceBag::clear()
    {
        if ( !m_clips.empty() || !m_images.empty() || !m_failedImagePaths.empty() ) bumpGeneration();
        m_clips.clear();
        m_clipsByPath.clear();
        m_clipPaths.clear();
        m_images.clear();
        m_imagesByPath.clear();
        m_imagePaths.clear();
        m_failedImagePaths.clear();
    }
}  // namespace Tomos
