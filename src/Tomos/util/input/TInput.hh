#pragma once

#include <map>
#include <string>

namespace Tomos
{
    class TInput
    {
    public:
        bool isKeyDown( int p_keycode );

        bool                      isMouseDown( int p_button );
        double                    getMouseX();
        double                    getMouseY();
        std::pair<double, double> getMousePos();
        std::pair<double, double> getMouseDelta();

    private:
        bool                      m_firstMouseMove = true;
        std::pair<double, double> m_mousePosOld;
    };

}  // namespace Tomos
