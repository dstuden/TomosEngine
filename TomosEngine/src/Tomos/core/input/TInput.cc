#include "Tomos/core/input/TInput.hh"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <cmath>

#include "Tomos/core/app/TApplication.hh"

namespace Tomos
{
    namespace
    {
        GLFWwindow* defaultWindow() { return TApplication::get().window().getNativeWindow(); }

        bool blocked( const TInput::BlockFn& p_block ) { return static_cast<bool>( p_block ) && p_block(); }

        struct TMouseFrame
        {
            double m_x        = 0.0;
            double m_y        = 0.0;
            double m_dx       = 0.0;
            double m_dy       = 0.0;
            bool   m_haveLast = false;
        };

        TMouseFrame& mouseFrame()
        {
            static TMouseFrame sFrame;
            return sFrame;
        }

        struct TGamepadCache
        {
            GLFWgamepadstate m_state{};
            bool             m_present = false;
        };

        TGamepadCache& gamepadCache( int p_jid )
        {
            static TGamepadCache sPads[ GLFW_JOYSTICK_LAST + 1 ];
            if ( p_jid < GLFW_JOYSTICK_1 || p_jid > GLFW_JOYSTICK_LAST ) p_jid = GLFW_JOYSTICK_1;
            return sPads[ p_jid ];
        }

        void refreshGamepads()
        {
            for ( int jid = GLFW_JOYSTICK_1; jid <= GLFW_JOYSTICK_LAST; ++jid )
            {
                TGamepadCache& pad = gamepadCache( jid );
                pad.m_present      = glfwJoystickPresent( jid ) == GLFW_TRUE && glfwJoystickIsGamepad( jid ) == GLFW_TRUE;
                if ( pad.m_present )
                    pad.m_present = glfwGetGamepadState( jid, &pad.m_state ) == GLFW_TRUE;
                else
                    pad.m_state = {};
            }
        }

        float applyDeadzone( float p_value, float p_deadzone )
        {
            const float dz = std::abs( p_deadzone );
            if ( dz <= 0.0f ) return p_value;
            if ( std::abs( p_value ) < dz ) return 0.0f;
            // Remap so |v|==dz → 0 and |v|==1 → ±1.
            const float sign = p_value < 0.0f ? -1.0f : 1.0f;
            return sign * ( std::abs( p_value ) - dz ) / ( 1.0f - dz );
        }
    }  // namespace

    void TInput::beginFrame( GLFWwindow* p_window )
    {
        if ( p_window == nullptr ) p_window = defaultWindow();

        TMouseFrame& frame = mouseFrame();
        double       x = 0.0, y = 0.0;
        if ( p_window != nullptr ) glfwGetCursorPos( p_window, &x, &y );

        if ( frame.m_haveLast )
        {
            frame.m_dx = x - frame.m_x;
            frame.m_dy = y - frame.m_y;
        }
        else
        {
            frame.m_dx       = 0.0;
            frame.m_dy       = 0.0;
            frame.m_haveLast = true;
        }
        frame.m_x = x;
        frame.m_y = y;

        refreshGamepads();
    }

    bool TInput::keyDown( int p_key, const BlockFn& p_block ) { return keyDown( defaultWindow(), p_key, p_block ); }

    bool TInput::keyDown( GLFWwindow* p_window, int p_key, const BlockFn& p_block )
    {
        if ( p_window == nullptr || blocked( p_block ) ) return false;
        return glfwGetKey( p_window, p_key ) == GLFW_PRESS;
    }

    bool TInput::keyPressed( int p_key, bool& p_wasDown, const BlockFn& p_block ) { return keyPressed( defaultWindow(), p_key, p_wasDown, p_block ); }

    bool TInput::keyPressed( GLFWwindow* p_window, int p_key, bool& p_wasDown, const BlockFn& p_block )
    {
        const bool raw = p_window != nullptr && glfwGetKey( p_window, p_key ) == GLFW_PRESS;
        const bool hit = raw && !p_wasDown && !blocked( p_block );
        p_wasDown      = raw;
        return hit;
    }

    void TInput::mousePosition( double& p_x, double& p_y, GLFWwindow* p_window )
    {
        if ( p_window == nullptr ) p_window = defaultWindow();
        p_x = 0.0;
        p_y = 0.0;
        if ( p_window != nullptr ) glfwGetCursorPos( p_window, &p_x, &p_y );
    }

    void TInput::mouseDelta( double& p_dx, double& p_dy )
    {
        const TMouseFrame& frame = mouseFrame();
        p_dx                     = frame.m_dx;
        p_dy                     = frame.m_dy;
    }

    bool TInput::mouseButtonDown( int p_button, const BlockFn& p_block ) { return mouseButtonDown( defaultWindow(), p_button, p_block ); }

    bool TInput::mouseButtonDown( GLFWwindow* p_window, int p_button, const BlockFn& p_block )
    {
        if ( p_window == nullptr || blocked( p_block ) ) return false;
        return glfwGetMouseButton( p_window, p_button ) == GLFW_PRESS;
    }

    bool TInput::mouseButtonPressed( int p_button, bool& p_wasDown, const BlockFn& p_block )
    {
        return mouseButtonPressed( defaultWindow(), p_button, p_wasDown, p_block );
    }

    bool TInput::mouseButtonPressed( GLFWwindow* p_window, int p_button, bool& p_wasDown, const BlockFn& p_block )
    {
        const bool raw = p_window != nullptr && glfwGetMouseButton( p_window, p_button ) == GLFW_PRESS;
        const bool hit = raw && !p_wasDown && !blocked( p_block );
        p_wasDown      = raw;
        return hit;
    }

    bool TInput::gamepadPresent( int p_jid )
    {
        if ( p_jid < GLFW_JOYSTICK_1 || p_jid > GLFW_JOYSTICK_LAST ) return false;
        return gamepadCache( p_jid ).m_present;
    }

    bool TInput::gamepadButtonDown( int p_button, int p_jid, const BlockFn& p_block )
    {
        if ( blocked( p_block ) ) return false;
        const TGamepadCache& pad = gamepadCache( p_jid );
        if ( !pad.m_present || p_button < 0 || p_button > GLFW_GAMEPAD_BUTTON_LAST ) return false;
        return pad.m_state.buttons[ p_button ] == GLFW_PRESS;
    }

    bool TInput::gamepadButtonPressed( int p_button, bool& p_wasDown, int p_jid, const BlockFn& p_block )
    {
        const TGamepadCache& pad = gamepadCache( p_jid );
        const bool           raw = pad.m_present && p_button >= 0 && p_button <= GLFW_GAMEPAD_BUTTON_LAST && pad.m_state.buttons[ p_button ] == GLFW_PRESS;
        const bool           hit = raw && !p_wasDown && !blocked( p_block );
        p_wasDown                = raw;
        return hit;
    }

    float TInput::gamepadAxis( int p_axis, int p_jid, float p_deadzone, const BlockFn& p_block )
    {
        if ( blocked( p_block ) ) return 0.0f;
        const TGamepadCache& pad = gamepadCache( p_jid );
        if ( !pad.m_present || p_axis < 0 || p_axis > GLFW_GAMEPAD_AXIS_LAST ) return 0.0f;
        return applyDeadzone( pad.m_state.axes[ p_axis ], p_deadzone );
    }

    TInputPoll::TInputPoll( BlockFn p_block ) : m_window( defaultWindow() ), m_block( std::move( p_block ) ) {}

    TInputPoll::TInputPoll( GLFWwindow* p_window, BlockFn p_block ) : m_window( p_window ), m_block( std::move( p_block ) ) {}

    bool TInputPoll::down( int p_key ) const { return TInput::keyDown( m_window, p_key, m_block ); }

    bool TInputPoll::down( int p_key, const BlockFn& p_block ) const { return TInput::keyDown( m_window, p_key, p_block ); }

    bool TInputPoll::pressed( int p_key, bool& p_wasDown ) const { return TInput::keyPressed( m_window, p_key, p_wasDown, m_block ); }

    bool TInputPoll::pressed( int p_key, bool& p_wasDown, const BlockFn& p_block ) const { return TInput::keyPressed( m_window, p_key, p_wasDown, p_block ); }

    bool TInputPoll::mouseDown( int p_button ) const { return TInput::mouseButtonDown( m_window, p_button, m_block ); }

    bool TInputPoll::mouseDown( int p_button, const BlockFn& p_block ) const { return TInput::mouseButtonDown( m_window, p_button, p_block ); }

    bool TInputPoll::mousePressed( int p_button, bool& p_wasDown ) const { return TInput::mouseButtonPressed( m_window, p_button, p_wasDown, m_block ); }

    bool TInputPoll::mousePressed( int p_button, bool& p_wasDown, const BlockFn& p_block ) const
    {
        return TInput::mouseButtonPressed( m_window, p_button, p_wasDown, p_block );
    }

    void TInputPoll::mousePosition( double& p_x, double& p_y ) const { TInput::mousePosition( p_x, p_y, m_window ); }
}  // namespace Tomos
