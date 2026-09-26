#include "Tomos/util/profile/TFrameProfiler.hh"

#if TOMOS_DEBUG

#include <utility>

namespace Tomos
{
    TFrameProfiler& TFrameProfiler::get()
    {
        static TFrameProfiler s_instance;
        return s_instance;
    }

    void TFrameProfiler::setCaptureEnabled( bool p_enabled ) { m_captureEnabled = p_enabled; }

    void TFrameProfiler::beginFrame()
    {
        if ( !m_captureEnabled ) return;
        m_current.clear();
        m_stack.clear();
    }

    void TFrameProfiler::endFrame()
    {
        if ( !m_captureEnabled ) return;
        // Drop any unbalanced scopes so the sealed frame is consistent.
        while ( !m_stack.empty() ) endScope();
        m_lastFrame = std::move( m_current );
        m_current.clear();
    }

    void TFrameProfiler::beginScope( const char* p_name )
    {
        if ( !m_captureEnabled ) return;

        TOpenScope open;
        open.name        = p_name != nullptr ? p_name : "?";
        open.start       = std::chrono::steady_clock::now();
        open.parent      = m_stack.empty() ? -1 : m_stack.back().sampleIndex;
        open.depth       = static_cast<int>( m_stack.size() );
        open.sampleIndex = static_cast<int>( m_current.size() );

        TCpuScopeSample sample;
        sample.name   = open.name;
        sample.parent = open.parent;
        sample.depth  = open.depth;
        m_current.push_back( sample );

        m_stack.push_back( open );
    }

    void TFrameProfiler::endScope()
    {
        if ( !m_captureEnabled || m_stack.empty() ) return;

        const TOpenScope open = m_stack.back();
        m_stack.pop_back();

        const auto end = std::chrono::steady_clock::now();
        const double ms =
                std::chrono::duration<double, std::milli>( end - open.start ).count();

        if ( open.sampleIndex >= 0 && open.sampleIndex < static_cast<int>( m_current.size() ) )
            m_current[ static_cast<size_t>( open.sampleIndex ) ].durationMs = ms;
    }
}  // namespace Tomos

#endif  // TOMOS_DEBUG
