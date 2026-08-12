#define GLFW_INCLUDE_NONE
#include "Tomos/util/time/TTime.hh"

#include <GLFW/glfw3.h>

namespace Tomos
{
    float TTime::now() { return static_cast<float>( glfwGetTime() ); }

    void TTime::tick()
    {
        const float time = now();
        if ( !m_hasLast )
        {
            m_hasLast     = true;
            m_startTime   = time;
            m_lastTime    = time;
            m_dt          = 0.0f;
            m_realElapsed = 0.0f;
            return;
        }

        m_dt          = time - m_lastTime;
        m_lastTime    = time;
        m_realElapsed = time - m_startTime;
        if ( !m_paused ) m_elapsed += m_dt;
    }
}  // namespace Tomos
