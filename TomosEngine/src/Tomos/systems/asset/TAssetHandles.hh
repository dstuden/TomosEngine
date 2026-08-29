#pragma once

#include <cstdint>
#include <string>

namespace Tomos
{
    // TAssetSystem owns glTF packages; TSceneResourceBag owns scene textures/audio.
    // Components borrow pointers and rebind after generation() bumps.

    using TResourceGeneration = uint32_t;

    struct TMeshAssetRef
    {
        std::string m_assetName;
        uint32_t    m_meshIdx     = 0;
        uint32_t    m_materialIdx = 0;

        [[nodiscard]] bool empty() const { return m_assetName.empty(); }
    };

    struct TClipAssetRef
    {
        std::string m_assetName;
        std::string m_clipName;

        [[nodiscard]] bool empty() const { return m_assetName.empty(); }
    };

    struct TBagTextureRef
    {
        std::string m_path;

        [[nodiscard]] bool empty() const { return m_path.empty(); }
    };

    // Path + playback options for GIF / animated WebP / video (and static textures via resolveTexture).
    struct TBagAnimatedTextureRef
    {
        std::string m_path;
        bool        m_looping = true;
        bool        m_playing = true;
        float       m_speed   = 1.0f;

        [[nodiscard]] bool empty() const { return m_path.empty(); }
    };
}  // namespace Tomos
