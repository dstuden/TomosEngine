#pragma once

#include "Tomos/systems/TComponent.hh"
#include "Tomos/systems/audio/TAudioClip.hh"

namespace Tomos
{
    class TAudioComponent : public TComponent
    {
    public:
        explicit TAudioComponent( const TAudioClip* p_clip = nullptr ) : m_clip( p_clip ) {}

        const TAudioClip* m_clip = nullptr;

        float m_volume = 1.0f;
        float m_pitch  = 1.0f;

        float m_minDistance = 1.0f;
        float m_maxDistance = 40.0f;

        bool m_looping     = false;
        bool m_spatial     = true;
        bool m_playOnStart = false;

        bool m_playing = false;

        void play( bool p_restart = true )
        {
            m_playing = m_clip != nullptr;
            m_restart = p_restart;
        }

        void stop()
        {
            m_playing = false;
            m_restart = false;
            m_stop    = true;
        }

        bool m_restart = false;
        bool m_stop    = false;
    };
}  // namespace Tomos
