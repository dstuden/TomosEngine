#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "Tomos/gpu/vulkan/TVkImage.hh"
#include "Tomos/systems/asset/TAssetHandles.hh"
#include "Tomos/systems/audio/TAudioClip.hh"
#include "Tomos/util/image/TAnimatedTexture.hh"

namespace Tomos
{
    class TVkGpu;

    // Owns per-scene textures/audio. See TAssetHandles.hh.
    class TSceneResourceBag
    {
    public:
        [[nodiscard]] TResourceGeneration generation() const { return m_generation; }

        TAudioClip*        addClip( std::unique_ptr<TAudioClip> p_clip );
        TAudioClip*        getOrCreateClip( const std::string& p_path, const std::string& p_name = {} );
        TAudioClip*        findClipByPath( const std::string& p_path ) const;
        const std::string* findClipPath( const TAudioClip* p_clip ) const;

        TVkImage*          addImage( const std::string& p_path, std::unique_ptr<TVkImage> p_image );
        TVkImage*          findImage( const std::string& p_path ) const;
        const std::string* findImagePath( const TVkImage* p_image ) const;

        // On failure returns gpu.missingTexture() without caching in ownership maps.
        TVkImage* loadImage( TVkGpu& p_gpu, const std::string& p_path );

        // New animated instance each call (independent playback). nullptr on failure.
        TAnimatedTexture* createAnimatedTexture( TVkGpu& p_gpu, const std::string& p_path, const TBagAnimatedTextureRef& p_opts = {} );

        TAnimatedTexture*       findAnimatedTexture( const TVkImage* p_image );
        const TAnimatedTexture* findAnimatedTexture( const TVkImage* p_image ) const;

        // Advance all animated textures and upload dirty frames (batched).
        void tickAnimatedTextures( TVkGpu& p_gpu, float p_dt );

        // Advance only animated textures whose TVkImage* appears in p_inUse.
        void tickAnimatedTextures( TVkGpu& p_gpu, float p_dt, const std::unordered_set<const TVkImage*>& p_inUse );

        // Unified resolver: static → loadImage; animated → createAnimatedTexture.
        // On failure returns gpu.missingTexture().
        TVkImage* resolveTexture( TVkGpu& p_gpu, const std::string& p_path, const TBagAnimatedTextureRef& p_opts = {} );

        void clear();

    private:
        void bumpGeneration() { ++m_generation; }

        TResourceGeneration                                m_generation = 1;
        std::vector<std::unique_ptr<TAudioClip>>           m_clips;
        std::unordered_map<std::string, TAudioClip*>       m_clipsByPath;
        std::vector<std::unique_ptr<TVkImage>>             m_images;
        std::unordered_map<std::string, TVkImage*>         m_imagesByPath;
        std::unordered_map<const TVkImage*, std::string>   m_imagePaths;
        std::unordered_map<const TAudioClip*, std::string> m_clipPaths;
        std::unordered_set<std::string>                    m_failedImagePaths;

        std::vector<std::unique_ptr<TAnimatedTexture>>             m_animatedTextures;
        std::unordered_map<const TVkImage*, TAnimatedTexture*>     m_animatedByImage;
        std::unordered_map<const TVkImage*, std::string>           m_animatedPaths;
        std::unordered_set<std::string>                            m_failedAnimatedPaths;
    };
}  // namespace Tomos
