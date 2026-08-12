#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace Tomos
{
    // Empty m_data = load failed.
    struct TRgbaPixels
    {
        std::vector<uint8_t> m_data;
        int                  m_width  = 0;
        int                  m_height = 0;

        [[nodiscard]] explicit    operator bool() const { return !m_data.empty(); }
        [[nodiscard]] const void* pixels() const { return m_data.data(); }
    };

    [[nodiscard]] TRgbaPixels loadRgbaFile( const std::string& p_path );
}  // namespace Tomos
