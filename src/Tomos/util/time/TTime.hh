#pragma once

namespace Tomos
{
    // dt() is 0 while paused; elapsed() stops advancing.
    class TTime
    {
    public:
        static constexpr float g_kDefaultFixedDt = 1.0f / 60.0f;

        void tick();

        void               setPaused( bool p_paused ) { m_paused = p_paused; }
        [[nodiscard]] bool isPaused() const { return m_paused; }

        [[nodiscard]] float dt() const { return m_paused ? 0.0f : m_dt; }
        [[nodiscard]] float realDt() const { return m_dt; }

        [[nodiscard]] float fixedDt() const { return m_fixedDt; }
        void                setFixedDt( float p_fixedDt ) { m_fixedDt = p_fixedDt; }

        [[nodiscard]] float elapsed() const { return m_elapsed; }
        [[nodiscard]] float realElapsed() const { return m_realElapsed; }

        [[nodiscard]] static float now();

    private:
        bool  m_paused      = false;
        bool  m_hasLast     = false;
        float m_lastTime    = 0.0f;
        float m_dt          = 0.0f;
        float m_fixedDt     = g_kDefaultFixedDt;
        float m_elapsed     = 0.0f;
        float m_realElapsed = 0.0f;
        float m_startTime   = 0.0f;
    };
}  // namespace Tomos
