#pragma once

#include <glm/glm.hpp>
#include <memory>

#include "Tomos/gpu/vulkan/TVkMesh.hh"

namespace Tomos
{
    class TVkGpu;

    namespace TMeshPrimitives
    {
        [[nodiscard]] std::unique_ptr<TVkMesh> makeBox( TVkGpu& p_gpu );
        [[nodiscard]] std::unique_ptr<TVkMesh> makePlane( TVkGpu& p_gpu );
        [[nodiscard]] std::unique_ptr<TVkMesh> makeUVSphere( TVkGpu& p_gpu, int p_segments = 24, int p_rings = 16 );
    }  // namespace TMeshPrimitives
}  // namespace Tomos
