#pragma once

#include <cstdint>

namespace Tomos
{
    enum class TBufUsage : uint32_t
    {
        None     = 0,
        CopySrc  = 1 << 0,
        CopyDst  = 1 << 1,
        Vertex   = 1 << 2,
        Index    = 1 << 3,
        Uniform  = 1 << 4,
        Storage  = 1 << 5,
        Indirect = 1 << 6,
        // Host-visible upload scratch — map/unmap per upload(), not kept mapped.
        Staging  = 1 << 7,
    };

    inline TBufUsage operator|( TBufUsage p_a, TBufUsage p_b ) { return static_cast<TBufUsage>( static_cast<uint32_t>( p_a ) | static_cast<uint32_t>( p_b ) ); }

    inline bool operator&( TBufUsage p_a, TBufUsage p_b ) { return ( static_cast<uint32_t>( p_a ) & static_cast<uint32_t>( p_b ) ) != 0; }

    enum class TImgUsage : uint32_t
    {
        None            = 0,
        CopySrc         = 1 << 0,
        CopyDst         = 1 << 1,
        Sampled         = 1 << 2,
        ColorAttachment = 1 << 3,
        DepthAttachment = 1 << 4,
        Storage         = 1 << 5,
    };

    inline TImgUsage operator|( TImgUsage p_a, TImgUsage p_b ) { return static_cast<TImgUsage>( static_cast<uint32_t>( p_a ) | static_cast<uint32_t>( p_b ) ); }

    inline bool operator&( TImgUsage p_a, TImgUsage p_b ) { return ( static_cast<uint32_t>( p_a ) & static_cast<uint32_t>( p_b ) ) != 0; }

    enum class TImgFormat
    {
        RGBA8Unorm,
        RGBA8Srgb,
        RGBA16Float,
        RG8Unorm,
        R8Unorm,
        R32Float,
        B8G8R8A8Unorm,
        B8G8R8A8Srgb,
        D32Float,
        D24UnormS8Uint,
        B10G11R11UFloat,
    };

    enum class TTexFilter
    {
        Nearest,
        Linear,
    };

    enum class TTexAddr
    {
        Clamp,
        Repeat,
        Mirror,
    };

    enum class TMatAlpha
    {
        Opaque,
        Mask,
        Blend,
    };
}  // namespace Tomos
