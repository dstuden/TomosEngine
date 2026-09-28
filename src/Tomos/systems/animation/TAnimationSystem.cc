#include "Tomos/systems/animation/TAnimationSystem.hh"

#include <algorithm>
#include <cmath>
#include <glm/gtc/quaternion.hpp>
#include <string>
#include <unordered_map>

#include "Tomos/core/scene/TSceneNode.hh"

namespace Tomos
{
    namespace
    {
        // Lower-bound index of the key at or before p_time; returns [i0, i1, t].
        struct TSampleKeys
        {
            size_t m_i0 = 0;
            size_t m_i1 = 0;
            float  m_t  = 0.0f;
        };

        TSampleKeys sampleKeys( const std::vector<float>& p_times, float p_time )
        {
            TSampleKeys out{};
            if ( p_times.empty() ) return out;
            if ( p_times.size() == 1 || p_time <= p_times.front() )
            {
                out.m_i0 = out.m_i1 = 0;
                return out;
            }
            if ( p_time >= p_times.back() )
            {
                out.m_i0 = out.m_i1 = p_times.size() - 1;
                return out;
            }

            const auto it  = std::upper_bound( p_times.begin(), p_times.end(), p_time );
            out.m_i1       = static_cast<size_t>( it - p_times.begin() );
            out.m_i0       = out.m_i1 - 1;
            const float t0 = p_times[ out.m_i0 ];
            const float t1 = p_times[ out.m_i1 ];
            out.m_t        = ( t1 > t0 ) ? ( p_time - t0 ) / ( t1 - t0 ) : 0.0f;
            return out;
        }

        void sampleChannel( const TAnimationChannel& p_ch, float p_time, TAnimationSystem::TJointPose& p_pose )
        {
            const TSampleKeys keys = sampleKeys( p_ch.m_times, p_time );
            switch ( p_ch.m_path )
            {
                case TAnimPath::Translation:
                    if ( p_ch.m_translations.empty() ) break;
                    p_pose.m_hasT = true;
                    p_pose.m_t    = ( keys.m_i0 == keys.m_i1 ) ? p_ch.m_translations[ keys.m_i0 ]
                                                               : glm::mix( p_ch.m_translations[ keys.m_i0 ], p_ch.m_translations[ keys.m_i1 ], keys.m_t );
                    break;
                case TAnimPath::Rotation:
                    if ( p_ch.m_rotations.empty() ) break;
                    p_pose.m_hasR = true;
                    p_pose.m_r    = ( keys.m_i0 == keys.m_i1 )
                                            ? p_ch.m_rotations[ keys.m_i0 ]
                                            : glm::normalize( glm::slerp( p_ch.m_rotations[ keys.m_i0 ], p_ch.m_rotations[ keys.m_i1 ], keys.m_t ) );
                    break;
                case TAnimPath::Scale:
                    if ( p_ch.m_scales.empty() ) break;
                    p_pose.m_hasS = true;
                    p_pose.m_s    = ( keys.m_i0 == keys.m_i1 ) ? p_ch.m_scales[ keys.m_i0 ]
                                                               : glm::mix( p_ch.m_scales[ keys.m_i0 ], p_ch.m_scales[ keys.m_i1 ], keys.m_t );
                    break;
            }
        }

        void sampleClip( const TAnimationClip& p_clip, float p_time, std::unordered_map<std::string, TAnimationSystem::TJointPose>& p_out )
        {
            for ( const TAnimationChannel& ch : p_clip.m_channels )
            {
                if ( ch.m_targetName.empty() ) continue;
                sampleChannel( ch, p_time, p_out[ ch.m_targetName ] );
            }
        }

        void advanceClipTime( const TAnimationClip* p_clip, float& p_time, float p_speed, bool p_looping, bool& p_playing, float p_dt )
        {
            if ( p_clip == nullptr ) return;
            p_time += p_dt * p_speed;
            if ( p_clip->m_duration <= 0.0f ) return;

            if ( p_looping )
            {
                p_time = std::fmod( p_time, p_clip->m_duration );
                if ( p_time < 0.0f ) p_time += p_clip->m_duration;
            }
            else if ( p_time >= p_clip->m_duration )
            {
                p_time    = p_clip->m_duration;
                p_playing = false;
            }
        }

        void bindClipJoints( TSceneNode& p_root, const TAnimationClip* p_clip, std::unordered_map<std::string, TSceneNode*>& p_cache )
        {
            if ( p_clip == nullptr ) return;
            for ( const TAnimationChannel& ch : p_clip->m_channels )
            {
                if ( ch.m_targetName.empty() ) continue;
                if ( p_cache.count( ch.m_targetName ) ) continue;
                p_cache.emplace( ch.m_targetName, p_root.findByName( ch.m_targetName ) );
            }
        }

        void ensureJointCache( TAnimatorComponent& p_ac, TSceneNode& p_root )
        {
            const TLevelStore* store    = p_root.store();
            const uint64_t     topology = store != nullptr ? store->topologyVersion() : 0;

            if ( p_ac.m_jointCacheTopology != 0 && p_ac.m_jointCacheTopology == topology && p_ac.m_clip == p_ac.m_jointsBoundClip &&
                 p_ac.m_fadeFromClip == p_ac.m_jointsBoundFadeClip )
                return;

            p_ac.m_jointCache.clear();
            bindClipJoints( p_root, p_ac.m_clip, p_ac.m_jointCache );
            bindClipJoints( p_root, p_ac.m_fadeFromClip, p_ac.m_jointCache );
            p_ac.m_jointsBoundClip     = p_ac.m_clip;
            p_ac.m_jointsBoundFadeClip = p_ac.m_fadeFromClip;
            p_ac.m_jointCacheTopology  = topology;
        }

        void writePose( std::unordered_map<std::string, TSceneNode*>& p_cache,
                        const std::unordered_map<std::string, TAnimationSystem::TJointPose>& p_from,
                        const std::unordered_map<std::string, TAnimationSystem::TJointPose>& p_to, float p_weight )
        {
            const float w = std::clamp( p_weight, 0.0f, 1.0f );

            auto writeJoint = [ & ]( const std::string& p_name, const TAnimationSystem::TJointPose* p_a, const TAnimationSystem::TJointPose* p_b )
            {
                const auto it = p_cache.find( p_name );
                if ( it == p_cache.end() || it->second == nullptr ) return;
                TSceneNode& joint = *it->second;

                const bool hasT = ( p_a && p_a->m_hasT ) || ( p_b && p_b->m_hasT );
                const bool hasR = ( p_a && p_a->m_hasR ) || ( p_b && p_b->m_hasR );
                const bool hasS = ( p_a && p_a->m_hasS ) || ( p_b && p_b->m_hasS );

                if ( hasT )
                {
                    const glm::vec3 ta = ( p_a && p_a->m_hasT ) ? p_a->m_t : ( ( p_b && p_b->m_hasT ) ? p_b->m_t : joint.m_transform.translation() );
                    const glm::vec3 tb = ( p_b && p_b->m_hasT ) ? p_b->m_t : ta;
                    joint.m_transform.setTranslation( ( w <= 0.0f ) ? ta : ( w >= 1.0f ) ? tb : glm::mix( ta, tb, w ) );
                }
                if ( hasR )
                {
                    const glm::quat ra = ( p_a && p_a->m_hasR ) ? p_a->m_r : ( ( p_b && p_b->m_hasR ) ? p_b->m_r : joint.m_transform.rotation() );
                    const glm::quat rb = ( p_b && p_b->m_hasR ) ? p_b->m_r : ra;
                    joint.m_transform.setRotation( ( w <= 0.0f ) ? ra : ( w >= 1.0f ) ? rb : glm::normalize( glm::slerp( ra, rb, w ) ) );
                }
                if ( hasS )
                {
                    const glm::vec3 sa = ( p_a && p_a->m_hasS ) ? p_a->m_s : ( ( p_b && p_b->m_hasS ) ? p_b->m_s : joint.m_transform.scale() );
                    const glm::vec3 sb = ( p_b && p_b->m_hasS ) ? p_b->m_s : sa;
                    joint.m_transform.setScale( ( w <= 0.0f ) ? sa : ( w >= 1.0f ) ? sb : glm::mix( sa, sb, w ) );
                }
            };

            if ( w >= 1.0f || p_from.empty() )
            {
                for ( const auto& [ name, pose ] : p_to ) writeJoint( name, nullptr, &pose );
                return;
            }
            if ( w <= 0.0f )
            {
                for ( const auto& [ name, pose ] : p_from ) writeJoint( name, &pose, nullptr );
                return;
            }

            std::unordered_map<std::string, char> seen;
            for ( const auto& [ name, poseA ] : p_from )
            {
                const auto itB = p_to.find( name );
                writeJoint( name, &poseA, itB != p_to.end() ? &itB->second : nullptr );
                seen[ name ] = 1;
            }
            for ( const auto& [ name, poseB ] : p_to )
            {
                if ( seen.count( name ) ) continue;
                writeJoint( name, nullptr, &poseB );
            }
        }
    }  // namespace

    void TAnimationSystem::componentCreated( TSceneNode& p_node, TComponent& p_component )
    {
        auto& ac           = dynamic_cast<TAnimatorComponent&>( p_component );
        ac.invalidateJointCache();
        m_animators[ &ac ] = &p_node;
    }

    void TAnimationSystem::componentDestroyed( TSceneNode& /*p_node*/, TComponent& p_component )
    {
        auto& ac = dynamic_cast<TAnimatorComponent&>( p_component );
        m_animators.erase( &ac );
    }

    void TAnimationSystem::update( float p_dt )
    {
        for ( auto& [ ac, node ] : m_animators )
        {
            if ( !ac->m_playing || ac->m_clip == nullptr ) continue;

            if ( ac->m_stateMachine.m_enabled )
            {
                const TAnimState* next = nullptr;
                float             fade = 0.25f;
                if ( ac->m_stateMachine.evaluate( next, fade ) && next != nullptr ) ac->applyState( *next, fade, true );
            }

            if ( ac->m_clip->m_channels.empty() && ( ac->m_fadeFromClip == nullptr || ac->m_fadeFromClip->m_channels.empty() ) ) continue;

            advanceClipTime( ac->m_clip, ac->m_time, ac->m_speed, ac->m_looping, ac->m_playing, p_dt );

            if ( ac->m_fadeFromClip != nullptr )
            {
                bool fadePlaying = true;
                advanceClipTime( ac->m_fadeFromClip, ac->m_fadeFromTime, ac->m_fadeFromSpeed, ac->m_fadeFromLooping, fadePlaying, p_dt );

                if ( ac->m_crossfadeDuration > 0.0f )
                {
                    ac->m_crossfadeElapsed += p_dt;
                    ac->m_blendWeight = std::clamp( ac->m_crossfadeElapsed / ac->m_crossfadeDuration, 0.0f, 1.0f );
                }
                else
                    ac->m_blendWeight = 1.0f;

                if ( ac->m_blendWeight >= 1.0f ) ac->clearCrossfade();
            }

            ensureJointCache( *ac, *node );

            m_fromPose.clear();
            m_toPose.clear();
            if ( ac->m_fadeFromClip != nullptr ) sampleClip( *ac->m_fadeFromClip, ac->m_fadeFromTime, m_fromPose );
            sampleClip( *ac->m_clip, ac->m_time, m_toPose );
            writePose( ac->m_jointCache, m_fromPose, m_toPose, ac->m_fadeFromClip != nullptr ? ac->m_blendWeight : 1.0f );
        }
    }
}  // namespace Tomos
