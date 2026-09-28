#pragma once

#include <cstdint>

namespace Tomos
{
    inline constexpr uint32_t g_kMaxInstances        = 10'000;
    inline constexpr uint32_t g_kMaxLights           = 128;
    inline constexpr uint32_t g_kMaxSprites          = 10'000;
    inline constexpr uint32_t g_kMaxParticles        = 65'536;
    inline constexpr uint32_t g_kMaxEmitters         = 64;
    inline constexpr uint32_t g_kMaxParticleTextures = 64;
    // Point lights with castShadow use 6 consecutive layers (cubemap faces).
    inline constexpr uint32_t g_kMaxShadowMaps = 16;
    inline constexpr uint32_t g_kShadowMapSize = 2048;

    inline constexpr uint32_t g_kClusterX            = 16;
    inline constexpr uint32_t g_kClusterY            = 9;
    inline constexpr uint32_t g_kClusterZ            = 24;
    inline constexpr uint32_t g_kClusterCount        = g_kClusterX * g_kClusterY * g_kClusterZ;
    inline constexpr uint32_t g_kMaxLightsPerCluster = 64;

    inline constexpr uint32_t g_kMaxBonesPerFrame = 16'384;
}  // namespace Tomos
