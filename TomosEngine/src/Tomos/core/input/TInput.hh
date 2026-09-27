#pragma once

#include <functional>

struct GLFWwindow;

namespace Tomos
{
    class TApplication;

    // glfwGet* bypasses the event system. BlockFn returns true → report "not pressed".
    // Edge detection tracks raw device state so held keys don't fire on unblock.
    // TApplication calls beginFrame() once per frame before mouseDelta().
    class TInput
    {
    public:
        using BlockFn = std::function<bool()>;

        // Re-sync cursor and zero delta (e.g. after setCursorMode so look doesn't jump).
        static void discardMouseDelta( GLFWwindow* p_window = nullptr );

        [[nodiscard]] static bool keyDown( int p_key, const BlockFn& p_block = {} );
        [[nodiscard]] static bool keyDown( GLFWwindow* p_window, int p_key, const BlockFn& p_block = {} );

        // Rising edge; updates p_wasDown from raw GLFW state every call.
        [[nodiscard]] static bool keyPressed( int p_key, bool& p_wasDown, const BlockFn& p_block = {} );
        [[nodiscard]] static bool keyPressed( GLFWwindow* p_window, int p_key, bool& p_wasDown, const BlockFn& p_block = {} );

        static void mousePosition( double& p_x, double& p_y, GLFWwindow* p_window = nullptr );
        static void mouseDelta( double& p_dx, double& p_dy );

        [[nodiscard]] static bool mouseButtonDown( int p_button, const BlockFn& p_block = {} );
        [[nodiscard]] static bool mouseButtonDown( GLFWwindow* p_window, int p_button, const BlockFn& p_block = {} );

        [[nodiscard]] static bool mouseButtonPressed( int p_button, bool& p_wasDown, const BlockFn& p_block = {} );
        [[nodiscard]] static bool mouseButtonPressed( GLFWwindow* p_window, int p_button, bool& p_wasDown, const BlockFn& p_block = {} );

        [[nodiscard]] static bool gamepadPresent( int p_jid = 0 );

        [[nodiscard]] static bool gamepadButtonDown( int p_button, int p_jid = 0, const BlockFn& p_block = {} );
        [[nodiscard]] static bool gamepadButtonPressed( int p_button, bool& p_wasDown, int p_jid = 0, const BlockFn& p_block = {} );

        // Applies deadzone; returns 0 when blocked / absent.
        [[nodiscard]] static float gamepadAxis( int p_axis, int p_jid = 0, float p_deadzone = 0.15f, const BlockFn& p_block = {} );

    private:
        friend class TApplication;

        static void beginFrame( GLFWwindow* p_window = nullptr );
    };

    class TInputPoll
    {
    public:
        using BlockFn = TInput::BlockFn;

        explicit TInputPoll( BlockFn p_block = {} );
        explicit TInputPoll( GLFWwindow* p_window, BlockFn p_block = {} );

        [[nodiscard]] bool down( int p_key ) const;
        [[nodiscard]] bool down( int p_key, const BlockFn& p_block ) const;

        [[nodiscard]] bool pressed( int p_key, bool& p_wasDown ) const;
        [[nodiscard]] bool pressed( int p_key, bool& p_wasDown, const BlockFn& p_block ) const;

        [[nodiscard]] bool mouseDown( int p_button ) const;
        [[nodiscard]] bool mouseDown( int p_button, const BlockFn& p_block ) const;

        [[nodiscard]] bool mousePressed( int p_button, bool& p_wasDown ) const;
        [[nodiscard]] bool mousePressed( int p_button, bool& p_wasDown, const BlockFn& p_block ) const;

        void mousePosition( double& p_x, double& p_y ) const;

    private:
        GLFWwindow* m_window = nullptr;
        BlockFn     m_block;
    };
}  // namespace Tomos
