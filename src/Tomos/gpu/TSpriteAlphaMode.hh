#pragma once

#include <cstdint>

namespace Tomos
{
    // Sprite analogue of TMatAlpha (Cutout ≈ Mask).
    enum class TSpriteAlphaMode : uint8_t
    {
        Cutout = 0,  // discard below cutoff, depth write; drawn with opaque
        Blend  = 1,  // sorted, no depth write; after blend meshes
    };
}  // namespace Tomos
