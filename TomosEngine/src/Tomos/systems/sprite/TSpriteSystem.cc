#include "Tomos/systems/sprite/TSpriteSystem.hh"

#include <algorithm>

#include "Tomos/core/scene/TSceneNode.hh"
#include "Tomos/gpu/vulkan/TVkPass.hh"
#include "Tomos/util/logger/TLogger.hh"
#include "Tomos/util/math/TFrustum.hh"
#include "Tomos/util/memory/TArenaAllocator.hh"
#include "Tomos/util/memory/TFrameAllocator.hh"

namespace Tomos
{
    void TSpriteSystem::componentCreated( TSceneNode& p_node, TComponent& p_component )
    {
        auto& sc         = dynamic_cast<TSpriteComponent&>( p_component );
        m_sprites[ &sc ] = &p_node;
    }

    void TSpriteSystem::componentDestroyed( TSceneNode& /*p_node*/, TComponent& p_component )
    {
        m_sprites.erase( &dynamic_cast<TSpriteComponent&>( p_component ) );
    }

    void TSpriteSystem::populate( TFrameState& p_state ) const
    {
        TOMOS_HEAP_PROBE( "sprite.populate" );

        p_state.m_sprites.clear();
        p_state.m_spriteBatches.clear();

        if ( m_sprites.empty() ) return;

        struct TEntry
        {
            const TSpriteComponent* m_sprite;
            glm::vec3               m_worldPos;
            float                   m_distSq;  // to camera — for back-to-front order
        };

        TArena&              arena = TFrameAllocator::get().arena();
        TArenaVector<TEntry> entries{ TArenaAllocator<TEntry>( arena ) };
        entries.reserve( m_sprites.size() );

        const glm::vec3 camPos = glm::vec3( p_state.m_viewInv[ 3 ] );

        glm::vec4 camPlanes[ 6 ];
        extractFrustumPlanes( p_state.m_viewProj, camPlanes );

        bool truncated = false;
        for ( const auto& [ sc, node ] : m_sprites )
        {
            if ( !sc->m_visible || sc->m_color.a <= 0.0f || sc->m_texture == nullptr ) continue;
            if ( entries.size() >= g_kMaxSprites )
            {
                truncated = true;
                break;
            }

            const glm::vec3 worldPos = glm::vec3( node->m_transform.getGlobalMatrix()[ 3 ] );

            const float halfExt = 0.5f * glm::max( sc->m_size.x, sc->m_size.y );
            const TAABB bounds{ worldPos - glm::vec3( halfExt ), worldPos + glm::vec3( halfExt ) };
            if ( !aabbIntersectsFrustum( bounds, camPlanes ) ) continue;

            const glm::vec3 toCam = worldPos - camPos;
            entries.push_back( { sc, worldPos, glm::dot( toCam, toCam ) } );
        }

        // Cutout by texture, then blend back-to-front (texture ties for batching).
        std::sort( entries.begin(), entries.end(),
                   []( const TEntry& p_a, const TEntry& p_b )
                   {
                       const bool blendA = p_a.m_sprite->m_alphaMode == TSpriteAlphaMode::Blend;
                       const bool blendB = p_b.m_sprite->m_alphaMode == TSpriteAlphaMode::Blend;
                       if ( blendA != blendB ) return !blendA;
                       if ( blendA && p_a.m_distSq != p_b.m_distSq ) return p_a.m_distSq > p_b.m_distSq;
                       return p_a.m_sprite->m_texture < p_b.m_sprite->m_texture;
                   } );

        constexpr float kBlendDiscard = 1.0f / 255.0f;

        const TVkImage* currentTex   = nullptr;
        bool            currentBlend = false;
        for ( const TEntry& e : entries )
        {
            const bool blend = e.m_sprite->m_alphaMode == TSpriteAlphaMode::Blend;
            if ( p_state.m_spriteBatches.empty() || e.m_sprite->m_texture != currentTex || blend != currentBlend )
            {
                currentTex   = e.m_sprite->m_texture;
                currentBlend = blend;
                p_state.m_spriteBatches.push_back( { currentTex, static_cast<uint32_t>( p_state.m_sprites.size() ), 0, blend } );
            }

            TSpriteData data{};
            data.m_position    = e.m_worldPos;
            data.m_rotation    = e.m_sprite->m_rotation;
            data.m_size        = e.m_sprite->m_size;
            data.m_uvMin       = e.m_sprite->m_uvMin;
            data.m_uvMax       = e.m_sprite->m_uvMax;
            data.m_mode        = static_cast<uint32_t>( e.m_sprite->m_mode );
            data.m_alphaCutoff = blend ? kBlendDiscard : glm::clamp( e.m_sprite->m_alphaCutoff, kBlendDiscard, 1.0f );
            data.m_color       = e.m_sprite->m_color;

            p_state.m_sprites.push_back( data );
            ++p_state.m_spriteBatches.back().m_count;
        }

        if ( truncated ) TLOG_WARN() << "[TSpriteSystem] Sprite cap reached (" << g_kMaxSprites << ") — dropping remaining sprites";
    }
}  // namespace Tomos
