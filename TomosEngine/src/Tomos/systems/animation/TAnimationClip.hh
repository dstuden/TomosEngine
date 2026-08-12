#pragma once

#include <cstdint>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <string>
#include <vector>

#include "Tomos/gpu/TRenderLimits.hh"

namespace Tomos
{
    inline constexpr uint32_t k_maxBonesPerSkin = 128;

    enum class TAnimPath : uint8_t
    {
        Translation = 0,
        Rotation    = 1,
        Scale       = 2,
    };

    // Shared clips must not hold scene-node pointers.
    struct TAnimationChannel
    {
        std::string            m_targetName;
        TAnimPath              m_path = TAnimPath::Translation;
        std::vector<float>     m_times;
        std::vector<glm::vec3> m_translations;
        std::vector<glm::quat> m_rotations;
        std::vector<glm::vec3> m_scales;
    };

    struct TAnimationClip
    {
        std::string                    m_name;
        float                          m_duration = 0.0f;
        std::vector<TAnimationChannel> m_channels;
    };
}  // namespace Tomos
