#pragma once

namespace Tomos
{
    class TTime
    {
    public:
        TTime() = default;

        float deltaTime() const { return m_deltaTime; }
        float gameTime() const { return m_gameTime; }
        float deltaTimeMilli() const { return m_deltaTime * 1000.0f; }
        float gameTimeMilli() const { return m_gameTime * 1000.0f; }
        float lastFrame() const { return m_lastFrame; }
        float lastFrameMilli() const { return m_lastFrame * 1000.0f; }

        void update( float p_now )
        {
            m_deltaTime = p_now - m_lastFrame;
            m_lastFrame = p_now;
            m_gameTime += m_deltaTime;
        }

    private:
        float m_deltaTime = 0.0f;
        float m_gameTime  = 0.0f;
        float m_lastFrame = 0.0f;
    };
} // Tomos
