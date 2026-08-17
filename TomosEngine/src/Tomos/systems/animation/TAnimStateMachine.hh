#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "Tomos/systems/animation/TAnimationClip.hh"

namespace Tomos
{
    struct TAnimState
    {
        std::string           m_name;
        const TAnimationClip* m_clip    = nullptr;
        float                 m_speed   = 1.0f;
        bool                  m_looping = true;
    };

    struct TAnimTransition
    {
        std::string m_from;
        std::string m_to;
        std::string m_boolParam;
        bool        m_boolValue    = true;
        float       m_fadeDuration = 0.25f;
    };

    class TAnimStateMachine
    {
    public:
        bool                         m_enabled = false;
        std::vector<TAnimState>      m_states;
        std::vector<TAnimTransition> m_transitions;
        std::string                  m_current;

        void clear()
        {
            m_states.clear();
            m_transitions.clear();
            m_bools.clear();
            m_current.clear();
            m_pendingRequest.clear();
            m_enabled = false;
        }

        TAnimState& addState( std::string p_name, const TAnimationClip* p_clip, float p_speed = 1.0f, bool p_looping = true )
        {
            TAnimState s;
            s.m_name    = std::move( p_name );
            s.m_clip    = p_clip;
            s.m_speed   = p_speed;
            s.m_looping = p_looping;
            m_states.push_back( std::move( s ) );
            return m_states.back();
        }

        void addTransition( std::string p_from, std::string p_to, std::string p_boolParam, bool p_boolValue, float p_fadeDuration = 0.25f )
        {
            TAnimTransition t;
            t.m_from         = std::move( p_from );
            t.m_to           = std::move( p_to );
            t.m_boolParam    = std::move( p_boolParam );
            t.m_boolValue    = p_boolValue;
            t.m_fadeDuration = p_fadeDuration;
            m_transitions.push_back( std::move( t ) );
        }

        void setBool( const std::string& p_name, bool p_value ) { m_bools[ p_name ] = p_value; }

        [[nodiscard]] bool getBool( const std::string& p_name, bool p_default = false ) const
        {
            const auto it = m_bools.find( p_name );
            return it != m_bools.end() ? it->second : p_default;
        }

        void requestState( std::string p_name ) { m_pendingRequest = std::move( p_name ); }

        [[nodiscard]] const TAnimState* findState( const std::string& p_name ) const
        {
            for ( const TAnimState& s : m_states )
                if ( s.m_name == p_name ) return &s;
            return nullptr;
        }

        bool start( const std::string& p_initial )
        {
            if ( findState( p_initial ) == nullptr ) return false;
            m_current = p_initial;
            m_pendingRequest.clear();
            m_enabled = true;
            return true;
        }

        bool evaluate( const TAnimState*& p_outNext, float& p_outFade )
        {
            p_outNext = nullptr;
            p_outFade = 0.25f;
            if ( !m_enabled || m_states.empty() ) return false;

            if ( !m_pendingRequest.empty() )
            {
                const std::string req = std::move( m_pendingRequest );
                m_pendingRequest.clear();
                if ( req == m_current ) return false;
                const TAnimState* next = findState( req );
                if ( next == nullptr ) return false;
                p_outFade = fadeFor( m_current, req );
                p_outNext = next;
                m_current = req;
                return true;
            }

            for ( const TAnimTransition& t : m_transitions )
            {
                if ( !t.m_from.empty() && t.m_from != m_current ) continue;
                if ( t.m_to == m_current ) continue;
                if ( !t.m_boolParam.empty() && getBool( t.m_boolParam ) != t.m_boolValue ) continue;
                const TAnimState* next = findState( t.m_to );
                if ( next == nullptr ) continue;
                p_outNext = next;
                p_outFade = t.m_fadeDuration;
                m_current = t.m_to;
                return true;
            }
            return false;
        }

    private:
        std::unordered_map<std::string, bool> m_bools;
        std::string                           m_pendingRequest;

        [[nodiscard]] float fadeFor( const std::string& p_from, const std::string& p_to ) const
        {
            for ( const TAnimTransition& t : m_transitions )
            {
                if ( t.m_to != p_to ) continue;
                if ( !t.m_from.empty() && t.m_from != p_from ) continue;
                return t.m_fadeDuration;
            }
            return 0.25f;
        }
    };
}  // namespace Tomos
