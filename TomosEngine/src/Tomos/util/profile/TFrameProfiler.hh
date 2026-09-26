#pragma once

#include <chrono>
#include <cstdint>
#include <vector>

#ifndef TOMOS_DEBUG
#define TOMOS_DEBUG 0
#endif

#if TOMOS_DEBUG

namespace Tomos
{
    struct TCpuScopeSample
    {
        const char* name       = nullptr;
        double      durationMs = 0.0;
        int         parent     = -1;
        int         depth      = 0;
    };

    // Debug-only CPU frame profiler. Recording is a no-op unless capture is enabled.
    class TFrameProfiler
    {
    public:
        static TFrameProfiler& get();

        void               setCaptureEnabled( bool p_enabled );
        [[nodiscard]] bool isCaptureEnabled() const { return m_captureEnabled; }

        void beginFrame();
        void endFrame();

        void beginScope( const char* p_name );
        void endScope();

        [[nodiscard]] const std::vector<TCpuScopeSample>& lastFrame() const { return m_lastFrame; }

    private:
        struct TOpenScope
        {
            const char*                           name         = nullptr;
            std::chrono::steady_clock::time_point start{};
            int                                   sampleIndex  = -1;
            int                                   parent       = -1;
            int                                   depth        = 0;
        };

        bool                         m_captureEnabled = false;
        std::vector<TCpuScopeSample> m_current;
        std::vector<TCpuScopeSample> m_lastFrame;
        std::vector<TOpenScope>      m_stack;
    };

    class TProfileScope
    {
    public:
        explicit TProfileScope( const char* p_name )
        {
            if ( !TFrameProfiler::get().isCaptureEnabled() ) return;
            m_active = true;
            TFrameProfiler::get().beginScope( p_name );
        }

        ~TProfileScope()
        {
            if ( m_active ) TFrameProfiler::get().endScope();
        }

        TProfileScope( const TProfileScope& )            = delete;
        TProfileScope& operator=( const TProfileScope& ) = delete;

    private:
        bool m_active = false;
    };
}  // namespace Tomos

#endif  // TOMOS_DEBUG
