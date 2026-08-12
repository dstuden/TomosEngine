#include "Tomos/systems/audio/TAudioSystem.hh"

#include <cstring>
#include <miniaudio.h>

#include "Tomos/core/scene/TSceneNode.hh"
#include "Tomos/util/logger/TLogger.hh"
#include "Tomos/util/path/TPath.hh"

namespace Tomos
{
    struct TAudioSystem::TVoice
    {
        ma_sound m_sound{};
        bool     m_inited  = false;
        bool     m_started = false;  // true after the first ma_sound_start
    };

    TAudioSystem::TAudioSystem()
    {
        auto* engine = new ma_engine();
        std::memset( engine, 0, sizeof( ma_engine ) );

        ma_engine_config config = ma_engine_config_init();
        // One listener; Y-up world to match the rest of Tomos / glTF.
        config.listenerCount = 1;

        const ma_result result = ma_engine_init( &config, engine );
        if ( result != MA_SUCCESS )
        {
            TLOG_ERROR() << "[TAudioSystem] ma_engine_init failed (" << static_cast<int>( result ) << ")";
            delete engine;
            m_engine = nullptr;
            m_ready  = false;
            return;
        }

        m_engine = engine;
        m_ready  = true;
        ma_engine_listener_set_world_up( engine, 0, 0.0f, 1.0f, 0.0f );
        ma_engine_set_volume( engine, m_masterVolume );
        TLOG_INFO() << "[TAudioSystem] Audio engine ready";
    }

    TAudioSystem::~TAudioSystem()
    {
        for ( auto& [ comp, voice ] : m_voices )
        {
            ( void ) comp;
            if ( voice && voice->m_inited )
            {
                ma_sound_uninit( &voice->m_sound );
                voice->m_inited = false;
            }
        }
        m_voices.clear();
        m_emitters.clear();

        if ( m_engine != nullptr )
        {
            ma_engine_uninit( static_cast<ma_engine*>( m_engine ) );
            delete static_cast<ma_engine*>( m_engine );
            m_engine = nullptr;
        }
        m_ready = false;
    }

    void TAudioSystem::componentCreated( TSceneNode& p_node, TComponent& p_component )
    {
        auto& ac          = static_cast<TAudioComponent&>( p_component );
        m_emitters[ &ac ] = &p_node;
        if ( ac.m_playOnStart && ac.m_clip != nullptr ) ac.play( true );
    }

    void TAudioSystem::componentDestroyed( TSceneNode& /*p_node*/, TComponent& p_component )
    {
        auto& ac = static_cast<TAudioComponent&>( p_component );
        destroyVoice( &ac );
        m_emitters.erase( &ac );
    }

    void TAudioSystem::destroyVoice( TAudioComponent* p_comp )
    {
        const auto it = m_voices.find( p_comp );
        if ( it == m_voices.end() ) return;
        if ( it->second && it->second->m_inited )
        {
            ma_sound_uninit( &it->second->m_sound );
            it->second->m_inited = false;
        }
        m_voices.erase( it );
    }

    bool TAudioSystem::ensureVoice( TAudioComponent* p_comp )
    {
        if ( !m_ready || m_engine == nullptr || p_comp->m_clip == nullptr ) return false;

        auto it = m_voices.find( p_comp );
        if ( it != m_voices.end() && it->second && it->second->m_inited ) return true;

        auto  voice  = std::make_unique<TVoice>();
        auto* engine = static_cast<ma_engine*>( m_engine );

        ma_uint32 flags = MA_SOUND_FLAG_DECODE;
        if ( !p_comp->m_spatial ) flags |= MA_SOUND_FLAG_NO_SPATIALIZATION;

        const std::string filePath = TPath::resolveString( p_comp->m_clip->m_path );
        const ma_result   result   = ma_sound_init_from_file( engine, filePath.c_str(), flags, nullptr, nullptr, &voice->m_sound );
        if ( result != MA_SUCCESS )
        {
            TLOG_ERROR() << "[TAudioSystem] Failed to load '" << p_comp->m_clip->m_path << "' (" << static_cast<int>( result ) << ")";
            return false;
        }

        voice->m_inited    = true;
        m_voices[ p_comp ] = std::move( voice );
        return true;
    }

    void TAudioSystem::syncVoiceParams( TAudioComponent* p_comp, TSceneNode* p_node )
    {
        const auto it = m_voices.find( p_comp );
        if ( it == m_voices.end() || !it->second || !it->second->m_inited ) return;

        ma_sound& sound = it->second->m_sound;
        ma_sound_set_volume( &sound, p_comp->m_volume );
        ma_sound_set_pitch( &sound, p_comp->m_pitch );
        ma_sound_set_looping( &sound, p_comp->m_looping ? MA_TRUE : MA_FALSE );

        if ( p_comp->m_spatial && p_node != nullptr )
        {
            const glm::vec3 pos = glm::vec3( p_node->m_transform.getGlobalMatrix()[ 3 ] );
            ma_sound_set_position( &sound, pos.x, pos.y, pos.z );
            ma_sound_set_min_distance( &sound, p_comp->m_minDistance );
            ma_sound_set_max_distance( &sound, p_comp->m_maxDistance );
        }
    }

    void TAudioSystem::lateUpdate( float /*p_dt*/ )
    {
        if ( !m_ready ) return;

        for ( auto& [ ac, node ] : m_emitters )
        {
            if ( ac->m_stop )
            {
                destroyVoice( ac );
                ac->m_stop    = false;
                ac->m_restart = false;
                ac->m_playing = false;
                continue;
            }

            if ( !ac->m_playing || ac->m_clip == nullptr )
            {
                const auto vit = m_voices.find( ac );
                if ( vit != m_voices.end() && vit->second && vit->second->m_inited )
                {
                    if ( !ma_sound_is_playing( &vit->second->m_sound ) ) destroyVoice( ac );
                }
                continue;
            }

            if ( ac->m_restart )
            {
                destroyVoice( ac );
                ac->m_restart = false;
            }

            if ( !ensureVoice( ac ) )
            {
                ac->m_playing = false;
                continue;
            }

            syncVoiceParams( ac, node );

            const auto vit = m_voices.find( ac );
            if ( vit == m_voices.end() || !vit->second || !vit->second->m_inited ) continue;

            TVoice&   voice = *vit->second;
            ma_sound& sound = voice.m_sound;

            if ( !voice.m_started )
            {
                const ma_result startResult = ma_sound_start( &sound );
                if ( startResult != MA_SUCCESS )
                {
                    TLOG_ERROR() << "[TAudioSystem] ma_sound_start failed (" << static_cast<int>( startResult ) << ")";
                    ac->m_playing = false;
                    destroyVoice( ac );
                    continue;
                }
                voice.m_started = true;
            }
            else if ( !ac->m_looping && !ma_sound_is_playing( &sound ) )
            {
                ac->m_playing = false;
                destroyVoice( ac );
            }
        }
    }

    void TAudioSystem::updateListener( const glm::vec3& p_position, const glm::vec3& p_forward, const glm::vec3& p_up )
    {
        if ( !m_ready || m_engine == nullptr ) return;

        auto* engine = static_cast<ma_engine*>( m_engine );
        ma_engine_listener_set_position( engine, 0, p_position.x, p_position.y, p_position.z );

        glm::vec3 forward = p_forward;
        if ( glm::dot( forward, forward ) < 1e-8f )
            forward = { 0.0f, 0.0f, -1.0f };
        else
            forward = glm::normalize( forward );

        glm::vec3 up = p_up;
        if ( glm::dot( up, up ) < 1e-8f )
            up = { 0.0f, 1.0f, 0.0f };
        else
            up = glm::normalize( up );

        ma_engine_listener_set_direction( engine, 0, forward.x, forward.y, forward.z );
        ma_engine_listener_set_world_up( engine, 0, up.x, up.y, up.z );
    }

    void TAudioSystem::setMasterVolume( float p_volume )
    {
        m_masterVolume = p_volume;
        if ( m_ready && m_engine != nullptr ) ma_engine_set_volume( static_cast<ma_engine*>( m_engine ), m_masterVolume );
    }
}  // namespace Tomos
