#pragma once

#include <cstdint>

namespace Tomos
{
    namespace TPhysicsLayer
    {
        constexpr uint32_t Default    = 1u << 0;
        constexpr uint32_t Static     = 1u << 1;
        constexpr uint32_t Dynamic    = 1u << 2;
        constexpr uint32_t Player     = 1u << 3;
        constexpr uint32_t Trigger    = 1u << 4;
        constexpr uint32_t Projectile = 1u << 5;
        constexpr uint32_t All        = 0xFFFFFFFFu;
    }  // namespace TPhysicsLayer
}  // namespace Tomos
