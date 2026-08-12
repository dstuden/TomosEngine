#pragma once

#include <memory>
#include <string>

#include "Tomos/core/scene/TSceneNode.hh"
#include "Tomos/systems/asset/TAssetSystem.hh"

namespace Tomos
{
    class TVkGpu;

    struct TLoadResult
    {
        std::shared_ptr<TSceneNode> m_root;
        std::unique_ptr<TGpuAsset>  m_asset;
    };

    class TGltfLoader
    {
    public:
        static TLoadResult load( const std::string& p_path, TVkGpu& p_gpu );
    };
}  // namespace Tomos
