#include "Tomos/util/profile/TGpuTimestamps.hh"

#if TOMOS_DEBUG

#include <stdexcept>

#include "Tomos/util/profile/TFrameProfiler.hh"

namespace Tomos
{
    void TGpuTimestamps::destroy( VkDevice p_device )
    {
        if ( m_pool != VK_NULL_HANDLE && p_device != VK_NULL_HANDLE )
        {
            vkDestroyQueryPool( p_device, m_pool, nullptr );
            m_pool = VK_NULL_HANDLE;
        }
        m_framesInFlight = 0;
        m_hasResults     = false;
        m_frameHadCapture.clear();
        m_lastPassMs.fill( 0.0f );
    }

    void TGpuTimestamps::ensureCreated( VkDevice p_device, uint32_t p_framesInFlight )
    {
        if ( m_pool != VK_NULL_HANDLE ) return;
        if ( p_device == VK_NULL_HANDLE || p_framesInFlight == 0 ) return;

        m_framesInFlight = p_framesInFlight;
        m_frameHadCapture.assign( p_framesInFlight, 0 );

        VkQueryPoolCreateInfo ci{};
        ci.sType      = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
        ci.queryType  = VK_QUERY_TYPE_TIMESTAMP;
        ci.queryCount = p_framesInFlight * g_kGpuQueriesPerFrame;

        if ( vkCreateQueryPool( p_device, &ci, nullptr, &m_pool ) != VK_SUCCESS )
            throw std::runtime_error( "[TGpuTimestamps] Failed to create query pool" );
    }

    void TGpuTimestamps::resolvePrevious( VkDevice p_device, uint32_t p_frameIndex, float p_timestampPeriodNs )
    {
        if ( m_pool == VK_NULL_HANDLE || p_device == VK_NULL_HANDLE ) return;
        if ( p_frameIndex >= m_framesInFlight ) return;
        if ( !m_frameHadCapture[ p_frameIndex ] ) return;

        const uint32_t        first = p_frameIndex * g_kGpuQueriesPerFrame;
        std::vector<uint64_t> results( g_kGpuQueriesPerFrame );

        const VkResult r = vkGetQueryPoolResults( p_device, m_pool, first, g_kGpuQueriesPerFrame, results.size() * sizeof( uint64_t ), results.data(),
                                                  sizeof( uint64_t ), VK_QUERY_RESULT_64_BIT );
        if ( r != VK_SUCCESS ) return;

        for ( uint32_t i = 0; i < g_kGpuPassCount; ++i )
        {
            const uint64_t begin = results[ i * 2u ];
            const uint64_t end   = results[ i * 2u + 1u ];
            if ( end <= begin )
            {
                m_lastPassMs[ i ] = 0.0f;
                continue;
            }
            const double ns   = static_cast<double>( end - begin ) * static_cast<double>( p_timestampPeriodNs );
            m_lastPassMs[ i ] = static_cast<float>( ns / 1.0e6 );
        }
        m_hasResults = true;
    }

    void TGpuTimestamps::beginRecord( VkCommandBuffer p_cmd, uint32_t p_frameIndex )
    {
        if ( !TFrameProfiler::get().isCaptureEnabled() ) return;
        if ( m_pool == VK_NULL_HANDLE || p_cmd == VK_NULL_HANDLE ) return;
        if ( p_frameIndex >= m_framesInFlight ) return;

        const uint32_t first = p_frameIndex * g_kGpuQueriesPerFrame;
        vkCmdResetQueryPool( p_cmd, m_pool, first, g_kGpuQueriesPerFrame );
        m_frameHadCapture[ p_frameIndex ] = 1;
    }

    void TGpuTimestamps::writeBegin( VkCommandBuffer p_cmd, uint32_t p_frameIndex, TGpuPass p_pass )
    {
        if ( !TFrameProfiler::get().isCaptureEnabled() ) return;
        if ( m_pool == VK_NULL_HANDLE || p_cmd == VK_NULL_HANDLE ) return;
        if ( p_frameIndex >= m_framesInFlight ) return;

        vkCmdWriteTimestamp( p_cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, m_pool, queryIndex( p_frameIndex, p_pass, false ) );
    }

    void TGpuTimestamps::writeEnd( VkCommandBuffer p_cmd, uint32_t p_frameIndex, TGpuPass p_pass )
    {
        if ( !TFrameProfiler::get().isCaptureEnabled() ) return;
        if ( m_pool == VK_NULL_HANDLE || p_cmd == VK_NULL_HANDLE ) return;
        if ( p_frameIndex >= m_framesInFlight ) return;

        vkCmdWriteTimestamp( p_cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, m_pool, queryIndex( p_frameIndex, p_pass, true ) );
    }
}  // namespace Tomos

#endif  // TOMOS_DEBUG
