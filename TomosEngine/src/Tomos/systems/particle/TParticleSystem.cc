#include "Tomos/systems/particle/TParticleSystem.hh"

#include <algorithm>
#include <functional>
#include <unordered_set>
#include <vector>

#include "Tomos/core/scene/TSceneNode.hh"
#include "Tomos/gpu/vulkan/TVkPass.hh"
#include "Tomos/util/logger/TLogger.hh"

namespace Tomos
{
    TParticleSystem::TParticleSystem()
    {
        m_textures.push_back( nullptr );  // slot 0 = default white
    }

    void TParticleSystem::componentCreated( TSceneNode& p_node, TComponent& p_component )
    {
        auto& emitter          = dynamic_cast<TParticleEmitterComponent&>( p_component );
        m_emitters[ &emitter ] = &p_node;
        m_lastTextureFingerprint.reset();
    }

    void TParticleSystem::componentDestroyed( TSceneNode& /*p_node*/, TComponent& p_component )
    {
        m_emitters.erase( &dynamic_cast<TParticleEmitterComponent&>( p_component ) );
        m_lastTextureFingerprint.reset();
    }

    size_t TParticleSystem::computeTextureFingerprint() const
    {
        std::vector<const TVkImage*> textures;
        textures.reserve( m_emitters.size() );
        for ( const auto& [ emitter, node ] : m_emitters )
        {
            ( void ) node;
            if ( emitter->m_texture != nullptr ) textures.push_back( emitter->m_texture );
        }

        std::sort( textures.begin(), textures.end() );

        size_t hash = 0;
        for ( const TVkImage* tex : textures )
        {
            const size_t h = std::hash<const void*>{}( tex );
            hash ^= h + 0x9e3779b9 + ( hash << 6 ) + ( hash >> 2 );
        }
        return hash;
    }

    void TParticleSystem::syncTextureSlots()
    {
        std::unordered_set<const TVkImage*> live;
        live.reserve( m_emitters.size() );
        for ( const auto& [ emitter, node ] : m_emitters )
        {
            ( void ) node;
            if ( emitter->m_texture != nullptr ) live.insert( emitter->m_texture );
        }

        // Release slots no longer referenced by any emitter (indices stay reserved
        // in m_textures so lingering GPU particles keep a valid sampler until reuse).
        for ( auto it = m_texToIndex.begin(); it != m_texToIndex.end(); )
        {
            if ( live.find( it->first ) == live.end() )
            {
                m_freeSlots.push_back( it->second );
                it = m_texToIndex.erase( it );
            }
            else
            {
                ++it;
            }
        }

        for ( const TVkImage* tex : live ) ( void ) textureIndex( tex );
    }

    uint32_t TParticleSystem::textureIndex( const TVkImage* p_texture )
    {
        if ( p_texture == nullptr ) return 0;

        const auto it = m_texToIndex.find( p_texture );
        if ( it != m_texToIndex.end() ) return it->second;

        uint32_t idx = 0;
        if ( !m_freeSlots.empty() )
        {
            idx = m_freeSlots.back();
            m_freeSlots.pop_back();
            m_textures[ idx ] = p_texture;
        }
        else if ( m_textures.size() < g_kMaxParticleTextures )
        {
            idx = static_cast<uint32_t>( m_textures.size() );
            m_textures.push_back( p_texture );
        }
        else
        {
            if ( !m_overflowWarned )
            {
                TLOG_WARN() << "[TParticleSystem] Particle texture slots full (" << g_kMaxParticleTextures << ") — falling back to default sampler";
                m_overflowWarned = true;
            }
            return 0;
        }

        m_texToIndex.emplace( p_texture, idx );
        return idx;
    }

    void TParticleSystem::populate( TFrameState& p_state, float p_dt )
    {
        p_state.m_emitters.clear();
        p_state.m_particleDt = p_dt;
        m_overflowWarned     = false;

        const size_t fingerprint = computeTextureFingerprint();
        if ( !m_lastTextureFingerprint || *m_lastTextureFingerprint != fingerprint )
        {
            syncTextureSlots();
            m_lastTextureFingerprint = fingerprint;
        }
        p_state.m_particleTextures = m_textures;

        if ( m_emitters.empty() ) return;

        bool truncatedEmitters = false;
        for ( const auto& [ emitter, node ] : m_emitters )
        {
            if ( p_state.m_emitters.size() >= g_kMaxEmitters )
            {
                truncatedEmitters = true;
                break;
            }

            const uint32_t spawn = emitter->takeSpawnCount( p_dt );
            if ( spawn == 0 ) continue;

            TEmitterData data{};
            data.m_position    = glm::vec3( node->m_transform.getGlobalMatrix()[ 3 ] );
            data.m_gravity     = emitter->m_gravity;
            data.m_velocityMin = emitter->m_velocityMin;
            data.m_lifetimeMin = emitter->m_lifetimeMin;
            data.m_velocityMax = emitter->m_velocityMax;
            data.m_lifetimeMax = emitter->m_lifetimeMax;
            data.m_colorStart  = emitter->m_colorStart;
            data.m_colorEnd    = emitter->m_colorEnd;
            data.m_sizeStart   = emitter->m_sizeStart;
            data.m_sizeEnd     = emitter->m_sizeEnd;
            data.m_spawnCount  = spawn;
            data.m_seed        = emitter->m_seed++;
            data.m_texIndex    = textureIndex( emitter->m_texture );
            data.m_uvMin       = emitter->m_uvMin;
            data.m_uvMax       = emitter->m_uvMax;

            p_state.m_emitters.push_back( data );
        }

        if ( truncatedEmitters ) TLOG_WARN() << "[TParticleSystem] Emitter cap reached (" << g_kMaxEmitters << ") — dropping remaining emitters";

        p_state.m_particleTextures = m_textures;
    }
}  // namespace Tomos
