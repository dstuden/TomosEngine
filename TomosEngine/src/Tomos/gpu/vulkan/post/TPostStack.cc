#include "Tomos/gpu/vulkan/post/TPostStack.hh"

#include "Tomos/gpu/vulkan/post/TPostBloom.hh"
#include "Tomos/gpu/vulkan/post/TPostSAO.hh"
#include "Tomos/gpu/vulkan/post/TPostTonemap.hh"
#include "Tomos/util/profile/TProfile.hh"

namespace Tomos
{
    TPostStack::~TPostStack()
    {
        for ( auto& e : m_effects ) e->destroy();
        m_effects.clear();
    }

    void TPostStack::add( std::unique_ptr<TPostEffect> p_effect ) { m_effects.push_back( std::move( p_effect ) ); }

    void TPostStack::onResize( const TPostContext& p_ctx )
    {
        for ( auto& e : m_effects ) e->onResize( p_ctx );
    }

    void TPostStack::execute( VkCommandBuffer p_cmd, TPostContext& p_ctx )
    {
        // Optional effects first; tonemap always last (even if somehow disabled).
        TPostEffect* tonemap = nullptr;
        for ( auto& e : m_effects )
        {
            if ( dynamic_cast<TPostTonemap*>( e.get() ) )
            {
                tonemap = e.get();
                continue;
            }

#if TOMOS_DEBUG
            const bool isSao   = dynamic_cast<TPostSAO*>( e.get() ) != nullptr;
            const bool isBloom = dynamic_cast<TPostBloom*>( e.get() ) != nullptr;
            if ( isSao && p_ctx.m_gpuTimestamps )
                TOMOS_PROFILE_GPU_BEGIN( *p_ctx.m_gpuTimestamps, p_cmd, p_ctx.m_frameIndex, SAO );
            if ( isBloom && p_ctx.m_gpuTimestamps )
                TOMOS_PROFILE_GPU_BEGIN( *p_ctx.m_gpuTimestamps, p_cmd, p_ctx.m_frameIndex, Bloom );
#endif

            if ( e->m_enabled ) e->record( p_cmd, p_ctx );

#if TOMOS_DEBUG
            if ( isSao && p_ctx.m_gpuTimestamps )
                TOMOS_PROFILE_GPU_END( *p_ctx.m_gpuTimestamps, p_cmd, p_ctx.m_frameIndex, SAO );
            if ( isBloom && p_ctx.m_gpuTimestamps )
                TOMOS_PROFILE_GPU_END( *p_ctx.m_gpuTimestamps, p_cmd, p_ctx.m_frameIndex, Bloom );
#endif
        }

        if ( tonemap != nullptr )
        {
#if TOMOS_DEBUG
            if ( p_ctx.m_gpuTimestamps )
                TOMOS_PROFILE_GPU_BEGIN( *p_ctx.m_gpuTimestamps, p_cmd, p_ctx.m_frameIndex, Tonemap );
#endif
            tonemap->record( p_cmd, p_ctx );
#if TOMOS_DEBUG
            if ( p_ctx.m_gpuTimestamps )
                TOMOS_PROFILE_GPU_END( *p_ctx.m_gpuTimestamps, p_cmd, p_ctx.m_frameIndex, Tonemap );
#endif
        }
    }
}  // namespace Tomos
