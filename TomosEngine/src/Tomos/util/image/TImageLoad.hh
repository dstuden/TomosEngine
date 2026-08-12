#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace Tomos
{
    // RGBA8 pixels loaded from disk (stb). Empty m_data means load failed.
    struct TRgbaPixels
    {
        std::vector<uint8_t> m_data;
        int                  m_width  = 0;
        int                  m_height = 0;

        [[nodiscard]] explicit    operator bool() const { return !m_data.empty(); }
        [[nodiscard]] const void* pixels() const { return m_data.data(); }
    };

    // Load an image file as tightly packed RGBA8. Returns empty on failure.
    [[nodiscard]] TRgbaPixels loadRgbaFile( const std::string& p_path );
}  // namespace Tomos
