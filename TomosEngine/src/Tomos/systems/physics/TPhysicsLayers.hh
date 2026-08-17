#pragma once

#include <cstdint>


namespace Tomos::TPhysicsLayer
{
    constexpr uint32_t g_default    = 1u << 0;
    constexpr uint32_t g_static     = 1u << 1;
    constexpr uint32_t g_dynamic    = 1u << 2;
    constexpr uint32_t g_player     = 1u << 3;
    constexpr uint32_t g_trigger    = 1u << 4;
    constexpr uint32_t g_projectile = 1u << 5;
    constexpr uint32_t g_all        = 0xFFFFFFFFu;
}  // namespace Tomos::TPhysicsLayer
