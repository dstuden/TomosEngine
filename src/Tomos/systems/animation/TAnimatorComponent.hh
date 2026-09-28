#pragma once

#include <algorithm>
#include <cmath>
#include <memory>
#include <string>
#include <unordered_map>

#include "Tomos/systems/TComponent.hh"
#include "Tomos/systems/animation/TAnimStateMachine.hh"
#include "Tomos/systems/animation/TAnimationClip.hh"
#include "Tomos/systems/asset/TAssetHandles.hh"

namespace Tomos
{
    class TAssetSystem;
    class TSceneNode;

    // Clip borrowed from TAssetSystem (see TAssetHandles.hh).
    class TAnimatorComponent : public TComponent
    {
    public:
        TClipAssetRef         m_clipRef{};
        TResourceGeneration   m_boundGeneration = 0;
        const TAnimationClip* m_clip            = nullptr;
        float                 m_time            = 0.0f;
        float                 m_speed           = 1.0f;
        bool                  m_looping         = true;
        bool                  m_playing         = true;

        std::unordered_map<std::string, TSceneNode*> m_jointCache;
        uint64_t                                     m_jointCacheTopology = 0;  // 0 = never built
        const TAnimationClip* m_jointsBoundClip     = nullptr;
        const TAnimationClip* m_jointsBoundFadeClip = nullptr;

        void invalidateJointCache()
        {
            m_jointCacheTopology  = 0;
            m_jointsBoundClip     = nullptr;
            m_jointsBoundFadeClip = nullptr;
        }

        // Weight 0 = fully from, 1 = fully current.
        const TAnimationClip* m_fadeFromClip      = nullptr;
        float                 m_fadeFromTime      = 0.0f;
        float                 m_fadeFromSpeed     = 1.0f;
        bool                  m_fadeFromLooping   = true;
        float                 m_blendWeight       = 1.0f;
        float                 m_crossfadeDuration = 0.0f;
        float                 m_crossfadeElapsed  = 0.0f;

        float m_defaultFadeDuration = 0.25f;

        TAnimStateMachine m_stateMachine;

        void play( const TAnimationClip* p_clip, bool p_restart = true )
        {
            clearCrossfade();
            m_clip            = p_clip;
            m_playing         = p_clip != nullptr;
            m_clipRef         = {};
            m_boundGeneration = 0;
            if ( p_restart ) m_time = 0.0f;
        }

        void play( const TClipAssetRef& p_ref, const TAnimationClip* p_clip, TResourceGeneration p_generation, bool p_restart = true )
        {
            play( p_clip, p_restart );
            m_clipRef         = p_ref;
            m_boundGeneration = p_generation;
        }

        bool rebind( const TAssetSystem& p_assets );

        // Negative duration uses m_defaultFadeDuration; <= 0 hard-cuts.
        void crossfadeTo( const TAnimationClip* p_clip, float p_duration = -1.0f, bool p_restart = true )
        {
            if ( p_clip == nullptr )
            {
                stop();
                return;
            }

            const float duration = ( p_duration < 0.0f ) ? m_defaultFadeDuration : p_duration;
            if ( duration <= 0.0f || m_clip == nullptr || m_clip == p_clip )
            {
                play( p_clip, p_restart );
                return;
            }

            m_fadeFromClip    = m_clip;
            m_fadeFromTime    = m_time;
            m_fadeFromSpeed   = m_speed;
            m_fadeFromLooping = m_looping;

            m_clip              = p_clip;
            m_playing           = true;
            m_blendWeight       = 0.0f;
            m_crossfadeDuration = duration;
            m_crossfadeElapsed  = 0.0f;
            if ( p_restart ) m_time = 0.0f;
        }

        void stop()
        {
            m_playing = false;
            clearCrossfade();
        }

        void seek( float p_time )
        {
            m_time = p_time;
            wrapTime( m_time, m_clip, m_looping );
        }

        [[nodiscard]] bool isCrossfading() const { return m_fadeFromClip != nullptr && m_blendWeight < 1.0f; }

        void clearCrossfade()
        {
            m_fadeFromClip      = nullptr;
            m_fadeFromTime      = 0.0f;
            m_blendWeight       = 1.0f;
            m_crossfadeDuration = 0.0f;
            m_crossfadeElapsed  = 0.0f;
        }

        void applyState( const TAnimState& p_state, float p_fadeDuration, bool p_restart = true )
        {
            m_speed   = p_state.m_speed;
            m_looping = p_state.m_looping;
            if ( p_fadeDuration <= 0.0f || m_clip == nullptr )
                play( p_state.m_clip, p_restart );
            else
                crossfadeTo( p_state.m_clip, p_fadeDuration, p_restart );
        }

        static void wrapTime( float& p_time, const TAnimationClip* p_clip, bool p_looping )
        {
            if ( p_clip == nullptr || p_clip->m_duration <= 0.0f ) return;
            if ( p_looping )
            {
                p_time = std::fmod( p_time, p_clip->m_duration );
                if ( p_time < 0.0f ) p_time += p_clip->m_duration;
            }
            else
                p_time = std::clamp( p_time, 0.0f, p_clip->m_duration );
        }
    };

    inline TAnimationClip makeHoldPoseClip( const TAnimationClip& p_src, std::string p_name, float p_duration = 1.0f )
    {
        TAnimationClip out;
        out.m_name     = std::move( p_name );
        out.m_duration = p_duration > 0.0f ? p_duration : 1.0f;
        out.m_channels.reserve( p_src.m_channels.size() );

        for ( const TAnimationChannel& ch : p_src.m_channels )
        {
            TAnimationChannel hold;
            hold.m_targetName = ch.m_targetName;
            hold.m_path       = ch.m_path;
            hold.m_times      = { 0.0f };

            switch ( ch.m_path )
            {
                case TAnimPath::Translation:
                    if ( !ch.m_translations.empty() ) hold.m_translations = { ch.m_translations.front() };
                    break;
                case TAnimPath::Rotation:
                    if ( !ch.m_rotations.empty() ) hold.m_rotations = { ch.m_rotations.front() };
                    break;
                case TAnimPath::Scale:
                    if ( !ch.m_scales.empty() ) hold.m_scales = { ch.m_scales.front() };
                    break;
            }

            if ( hold.m_translations.empty() && hold.m_rotations.empty() && hold.m_scales.empty() ) continue;
            out.m_channels.push_back( std::move( hold ) );
        }
        return out;
    }
}  // namespace Tomos
