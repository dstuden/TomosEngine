//
// Created by dstuden on 1/21/25.
//

#include "../../core/TApplication.hh"
#include "TInput.hh"

namespace Tomos
{
    bool TInput::isKeyDown( int p_keycode )
    {
        auto state = glfwGetKey( TApplication::get()->getWindow().getNativeWindow(), p_keycode );

        if ( state == GLFW_PRESS || state == GLFW_REPEAT )
        {
            return true;
        }

        return false;
    }

    bool TInput::isMouseDown( int p_button )
    {
        auto state = glfwGetMouseButton( TApplication::get()->getWindow().getNativeWindow(), p_button );

        if ( state == GLFW_PRESS )
        {
            return true;
        }

        return false;
    }

    double TInput::getMouseX()
    {
        auto [x, y] = getMousePos();
        return x;
    }

    double TInput::getMouseY()
    {
        auto [x, y] = getMousePos();
        return y;
    }

    std::pair<double, double> TInput::getMousePos()
    {
        double x, y;
        glfwGetCursorPos( TApplication::get()->getWindow().getNativeWindow(), &x, &y );

        return {x, y};
    }

    std::pair<double, double> TInput::getMouseDelta()
    {
        if ( m_firstMouseMove )
        {
            m_mousePosOld    = getMousePos();
            m_firstMouseMove = false;
        }

        auto current = getMousePos();
        auto delta   = std::make_pair(
                current.first - m_mousePosOld.first,
                current.second - m_mousePosOld.second
                );
        m_mousePosOld = current; // Update for next call

        return delta;
    }
} // namespace Tomos
