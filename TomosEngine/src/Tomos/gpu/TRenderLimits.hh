#pragma once

#include <cstdint>

namespace Tomos
{
    inline constexpr uint32_t k_maxInstances        = 10'000;
    inline constexpr uint32_t k_maxLights           = 128;
    inline constexpr uint32_t k_maxSprites          = 10'000;
    inline constexpr uint32_t k_maxParticles        = 65'536;
    inline constexpr uint32_t k_maxEmitters         = 64;
    inline constexpr uint32_t k_maxParticleTextures = 64;
    // Point lights with castShadow use 6 consecutive layers (cubemap faces).
    inline constexpr uint32_t k_maxShadowMaps = 16;
    inline constexpr uint32_t k_shadowMapSize = 2048;

    inline constexpr uint32_t k_clusterX            = 16;
    inline constexpr uint32_t k_clusterY            = 9;
    inline constexpr uint32_t k_clusterZ            = 24;
    inline constexpr uint32_t k_clusterCount        = k_clusterX * k_clusterY * k_clusterZ;
    inline constexpr uint32_t k_maxLightsPerCluster = 64;

    inline constexpr uint32_t k_maxBonesPerFrame = 16'384;
}  // namespace Tomos
