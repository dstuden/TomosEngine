#include "Tomos/gpu/vulkan/post/TPostStack.hh"

#include "Tomos/gpu/vulkan/post/TPostTonemap.hh"

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
            if ( e->m_enabled ) e->record( p_cmd, p_ctx );
        }

        if ( tonemap != nullptr ) tonemap->record( p_cmd, p_ctx );
    }
}  // namespace Tomos
