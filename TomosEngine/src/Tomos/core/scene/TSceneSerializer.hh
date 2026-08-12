#pragma once

#include <string>
#include <unordered_set>

#include "Tomos/core/scene/TScene.hh"
#include "Tomos/systems/asset/TAssetSystem.hh"

namespace Tomos
{
    class TVkGpu;

    struct TSceneSerializeOpts
    {
        bool m_replaceChildren = true;
    };

    class TSceneSerializer
    {
    public:
        static bool saveToFile( const TScene& p_scene, const TAssetSystem& p_assets, const std::string& p_path );

        static bool loadFromFile( TScene& p_scene, TAssetSystem& p_assets, TVkGpu& p_gpu, const std::string& p_path, const TSceneSerializeOpts& p_opts = {} );
    };
}  // namespace Tomos
