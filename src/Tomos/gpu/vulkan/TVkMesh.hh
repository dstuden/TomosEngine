#pragma once

#include <cstdint>
#include <memory>

#include "Tomos/gpu/vulkan/TVkBuffer.hh"
#include "Tomos/util/math/TFrustum.hh"

namespace Tomos
{
    struct TVkMesh
    {
        std::unique_ptr<TVkBuffer> m_position;
        std::unique_ptr<TVkBuffer> m_texCoord;
        std::unique_ptr<TVkBuffer> m_normal;
        std::unique_ptr<TVkBuffer> m_tangent;
        std::unique_ptr<TVkBuffer> m_joints;
        std::unique_ptr<TVkBuffer> m_weights;
        std::unique_ptr<TVkBuffer> m_index;
        uint32_t                   m_drawCount    = 0;
        bool                       m_is16BitIndex = false;

        TAABB m_aabb{};

        [[nodiscard]] bool isIndexed() const { return m_index != nullptr; }
        [[nodiscard]] bool isSkinned() const { return m_joints != nullptr; }
        [[nodiscard]] bool hasTangents() const { return m_tangent != nullptr; }
    };
}  // namespace Tomos
