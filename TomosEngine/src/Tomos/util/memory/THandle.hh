#pragma once

#include <cstdint>
#include <limits>

namespace Tomos
{
    template<typename Tag>
    struct THandle
    {
        static constexpr uint32_t g_kInvalidIndex = std::numeric_limits<uint32_t>::max();

        uint32_t m_index      = g_kInvalidIndex;
        uint32_t m_generation = 0;

        [[nodiscard]] bool valid() const { return m_index != g_kInvalidIndex; }
        explicit           operator bool() const { return valid(); }

        bool operator==( const THandle& ) const = default;
        bool operator!=( const THandle& ) const = default;
    };

    struct TNodeTag;

    using TNodeHandle = THandle<TNodeTag>;
}  // namespace Tomos
