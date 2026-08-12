#pragma once

#include <cstdint>

namespace Tomos
{
    // Must match sprite.vert TSpriteData::m_mode.
    enum class TBillboardMode : uint32_t
    {
        Spherical   = 0,
        Cylindrical = 1,
        Fixed       = 2,
    };
}  // namespace Tomos
