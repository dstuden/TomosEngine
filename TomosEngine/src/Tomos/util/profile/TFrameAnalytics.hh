#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include "Tomos/util/memory/TFrameAllocator.hh"

#ifndef TOMOS_DEBUG
#define TOMOS_DEBUG 0
#endif

#if TOMOS_DEBUG

#include "Tomos/util/profile/TFrameProfiler.hh"
#include "Tomos/util/profile/TGpuTimestamps.hh"

namespace Tomos
{
    struct TGpuPassTiming
    {
        const char* name = nullptr;
        float       ms   = 0.0f;
    };

    struct TSceneCounts
    {
        size_t   drawCallsForward = 0;
        size_t   drawCallsShadow  = 0;
        size_t   instances        = 0;
        size_t   lights           = 0;
        size_t   sprites          = 0;
        size_t   spriteBatches    = 0;
        size_t   emitters         = 0;
        size_t   bones            = 0;
        uint32_t meshesTotal      = 0;
        uint32_t meshesCulled     = 0;
    };

    // Merged last-captured-frame snapshot for the Performance panel.
    struct TFrameAnalyticsSnapshot
    {
        float                       realDtMs = 0.0f;
        float                       simDtMs  = 0.0f;
        std::vector<TCpuScopeSample> cpu;
        std::array<TGpuPassTiming, g_kGpuPassCount> gpu{};
        bool                        gpuValid = false;
        TFrameMemoryStats           memory{};
        TSceneCounts                scene{};
    };

    class TFrameAnalytics
    {
    public:
        static TFrameAnalytics& get();

        // Pull CPU / GPU / memory / scene counts into m_last.
        void capture( float p_realDt, float p_simDt, const TSceneCounts& p_scene, const TFrameMemoryStats& p_memory,
                      const std::array<float, g_kGpuPassCount>* p_gpuPassMs, bool p_gpuValid );

        [[nodiscard]] const TFrameAnalyticsSnapshot& last() const { return m_last; }

    private:
        TFrameAnalyticsSnapshot m_last;
    };
}  // namespace Tomos

#endif  // TOMOS_DEBUG
