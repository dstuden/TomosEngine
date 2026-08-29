#include "Tomos/systems/sprite/TSpriteSystem.hh"

#include <algorithm>
#include <vector>

#include "Tomos/core/scene/TSceneNode.hh"
#include "Tomos/gpu/vulkan/TVkPass.hh"
#include "Tomos/util/logger/TLogger.hh"

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
        p_state.m_sprites.clear();
        p_state.m_spriteBatches.clear();

        if ( m_sprites.empty() ) return;

        struct TEntry
        {
            const TSpriteComponent* m_sprite;
            glm::vec3               m_worldPos;
            float                   m_distSq;  // to camera — for back-to-front order
        };

        std::vector<TEntry> entries;
        entries.reserve( m_sprites.size() );

        const glm::vec3 camPos = glm::vec3( p_state.m_viewInv[ 3 ] );

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
            const glm::vec3 toCam    = worldPos - camPos;
            entries.push_back( { sc, worldPos, glm::dot( toCam, toCam ) } );
        }

        // Back-to-front for correct alpha blending.  Equal-depth ties prefer the
        // same texture so consecutive runs still batch into one draw.
        std::sort( entries.begin(), entries.end(),
                   []( const TEntry& p_a, const TEntry& p_b )
                   {
                       if ( p_a.m_distSq != p_b.m_distSq ) return p_a.m_distSq > p_b.m_distSq;
                       return p_a.m_sprite->m_texture < p_b.m_sprite->m_texture;
                   } );

        const TVkImage* currentTex = nullptr;
        for ( const TEntry& e : entries )
        {
            if ( e.m_sprite->m_texture != currentTex || p_state.m_spriteBatches.empty() )
            {
                currentTex = e.m_sprite->m_texture;
                p_state.m_spriteBatches.push_back( { currentTex, static_cast<uint32_t>( p_state.m_sprites.size() ), 0 } );
            }

            TSpriteData data{};
            data.m_position = e.m_worldPos;
            data.m_rotation = e.m_sprite->m_rotation;
            data.m_size     = e.m_sprite->m_size;
            data.m_uvMin    = e.m_sprite->m_uvMin;
            data.m_uvMax    = e.m_sprite->m_uvMax;
            data.m_mode     = static_cast<uint32_t>( e.m_sprite->m_mode );
            data.m_color    = e.m_sprite->m_color;

            p_state.m_sprites.push_back( data );
            ++p_state.m_spriteBatches.back().m_count;
        }

        if ( truncated ) TLOG_WARN() << "[TSpriteSystem] Sprite cap reached (" << g_kMaxSprites << ") — dropping remaining sprites";
    }
}  // namespace Tomos
