#include "Tomos/util/profile/TFrameAnalytics.hh"

#if TOMOS_DEBUG

namespace Tomos
{
    TFrameAnalytics& TFrameAnalytics::get()
    {
        static TFrameAnalytics s_instance;
        return s_instance;
    }

    void TFrameAnalytics::capture( float p_realDt, float p_simDt, const TSceneCounts& p_scene, const TFrameMemoryStats& p_memory,
                                   const std::array<float, g_kGpuPassCount>* p_gpuPassMs, bool p_gpuValid )
    {
        m_last.realDtMs = p_realDt * 1000.0f;
        m_last.simDtMs  = p_simDt * 1000.0f;
        m_last.cpu      = TFrameProfiler::get().lastFrame();
        m_last.memory   = p_memory;
        m_last.scene    = p_scene;
        m_last.gpuValid = p_gpuValid && p_gpuPassMs != nullptr;

        for ( uint32_t i = 0; i < g_kGpuPassCount; ++i )
        {
            m_last.gpu[ i ].name = gpuPassName( static_cast<TGpuPass>( i ) );
            m_last.gpu[ i ].ms   = ( p_gpuPassMs != nullptr ) ? ( *p_gpuPassMs )[ i ] : 0.0f;
        }
    }
}  // namespace Tomos

#endif  // TOMOS_DEBUG
