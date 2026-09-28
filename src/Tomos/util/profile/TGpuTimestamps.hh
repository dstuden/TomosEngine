#pragma once

#include <array>
#include <cstdint>
#include <vector>
#include <vulkan/vulkan.h>

#include "Tomos/util/reflect/TReflectEnum.hh"

#ifndef TOMOS_DEBUG
#define TOMOS_DEBUG 0
#endif

#if TOMOS_DEBUG

namespace Tomos
{
    // GPU pass slots for timestamp queries. Post effects are split (SAO / Bloom / Tonemap).
    enum class TGpuPass : uint32_t
    {
        Shadows = 0,
        ClusterCull,
        ForwardOpaque,
        ForwardBlend,
        ParticleSim,
        ParticleDraw,
        SAO,
        Bloom,
        Tonemap,
        Count
    };

    inline constexpr uint32_t g_kGpuPassCount      = static_cast<uint32_t>( TGpuPass::Count );
    inline constexpr uint32_t g_kGpuQueriesPerFrame = g_kGpuPassCount * 2;  // begin + end

    inline const char* gpuPassName( TGpuPass p_pass )
    {
        if ( p_pass == TGpuPass::Count ) return "?";
        const auto name = Reflect::enumName( p_pass );
        return name.empty() ? "?" : name.data();
    }

    // Lazy VkQueryPool timestamps. Created on first capture; writes only while capturing.
    class TGpuTimestamps
    {
    public:
        void destroy( VkDevice p_device );

        // Call after the slot's fence has been waited (prior submission finished).
        void resolvePrevious( VkDevice p_device, uint32_t p_frameIndex, float p_timestampPeriodNs );

        // Reset this frame's query range before recording (capture on only).
        void beginRecord( VkCommandBuffer p_cmd, uint32_t p_frameIndex );

        void writeBegin( VkCommandBuffer p_cmd, uint32_t p_frameIndex, TGpuPass p_pass );
        void writeEnd( VkCommandBuffer p_cmd, uint32_t p_frameIndex, TGpuPass p_pass );

        void ensureCreated( VkDevice p_device, uint32_t p_framesInFlight );

        [[nodiscard]] const std::array<float, g_kGpuPassCount>& lastPassMs() const { return m_lastPassMs; }
        [[nodiscard]] bool                                      hasResults() const { return m_hasResults; }

    private:
        [[nodiscard]] uint32_t queryIndex( uint32_t p_frameIndex, TGpuPass p_pass, bool p_end ) const
        {
            return p_frameIndex * g_kGpuQueriesPerFrame + static_cast<uint32_t>( p_pass ) * 2u + ( p_end ? 1u : 0u );
        }

        VkQueryPool                             m_pool         = VK_NULL_HANDLE;
        uint32_t                                m_framesInFlight = 0;
        std::array<float, g_kGpuPassCount>      m_lastPassMs{};
        bool                                    m_hasResults   = false;
        std::vector<uint8_t>                    m_frameHadCapture;
    };
}  // namespace Tomos

#endif  // TOMOS_DEBUG
