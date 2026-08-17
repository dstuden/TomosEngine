#include "Tomos/core/input/TActionMap.hh"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <algorithm>
#include <cmath>

namespace Tomos
{
    void TActionMap::setActiveContext( TInputContext p_context ) { m_active = p_context; }

    void TActionMap::clear() { m_actions.clear(); }

    void TActionMap::bindKey( const std::string& p_action, int p_key, TInputContext p_context, float p_scale )
    {
        m_actions[ p_action ].push_back( TBinding{ .m_kind = TKind::Key, .m_code = p_key, .m_scale = p_scale, .m_context = p_context } );
    }

    void TActionMap::bindMouseButton( const std::string& p_action, int p_button, TInputContext p_context, float p_scale )
    {
        m_actions[ p_action ].push_back( TBinding{ .m_kind = TKind::MouseButton, .m_code = p_button, .m_scale = p_scale, .m_context = p_context } );
    }

    void TActionMap::bindGamepadButton( const std::string& p_action, int p_button, int p_jid, TInputContext p_context, float p_scale )
    {
        m_actions[ p_action ].push_back(
                TBinding{ .m_kind = TKind::GamepadButton, .m_code = p_button, .m_scale = p_scale, .m_jid = p_jid, .m_context = p_context } );
    }

    void TActionMap::bindGamepadAxis( const std::string& p_action, int p_axis, float p_scale, float p_deadzone, int p_jid, TInputContext p_context )
    {
        m_actions[ p_action ].push_back( TBinding{
                .m_kind = TKind::GamepadAxis, .m_code = p_axis, .m_scale = p_scale, .m_deadzone = p_deadzone, .m_jid = p_jid, .m_context = p_context } );
    }

    void TActionMap::bindKeyAxis( const std::string& p_action, int p_negativeKey, int p_positiveKey, TInputContext p_context )
    {
        bindKey( p_action, p_negativeKey, p_context, -1.0f );
        bindKey( p_action, p_positiveKey, p_context, 1.0f );
    }

    bool TActionMap::bindingActive( const TBinding& p_binding ) const { return p_binding.m_context == TInputContext::Any || p_binding.m_context == m_active; }

    bool TActionMap::bindingDigitalDown( const TBinding& p_binding, GLFWwindow* p_window, const BlockFn& p_block ) const
    {
        if ( !bindingActive( p_binding ) ) return false;

        switch ( p_binding.m_kind )
        {
            case TKind::Key:
                return TInput::keyDown( p_window, p_binding.m_code, p_block );
            case TKind::MouseButton:
                return TInput::mouseButtonDown( p_window, p_binding.m_code, p_block );
            case TKind::GamepadButton:
                return TInput::gamepadButtonDown( p_binding.m_code, p_binding.m_jid, p_block );
            case TKind::GamepadAxis:
                return std::abs( TInput::gamepadAxis( p_binding.m_code, p_binding.m_jid, p_binding.m_deadzone, p_block ) ) > 0.5f;
        }
        return false;
    }

    float TActionMap::bindingContribution( const TBinding& p_binding, GLFWwindow* p_window, const BlockFn& p_block ) const
    {
        if ( !bindingActive( p_binding ) ) return 0.0f;

        switch ( p_binding.m_kind )
        {
            case TKind::Key:
                return TInput::keyDown( p_window, p_binding.m_code, p_block ) ? p_binding.m_scale : 0.0f;
            case TKind::MouseButton:
                return TInput::mouseButtonDown( p_window, p_binding.m_code, p_block ) ? p_binding.m_scale : 0.0f;
            case TKind::GamepadButton:
                return TInput::gamepadButtonDown( p_binding.m_code, p_binding.m_jid, p_block ) ? p_binding.m_scale : 0.0f;
            case TKind::GamepadAxis:
                return TInput::gamepadAxis( p_binding.m_code, p_binding.m_jid, p_binding.m_deadzone, p_block ) * p_binding.m_scale;
        }
        return 0.0f;
    }

    bool TActionMap::down( const std::string& p_action, GLFWwindow* p_window, const BlockFn& p_block ) const
    {
        const auto it = m_actions.find( p_action );
        if ( it == m_actions.end() ) return false;

        for ( const TBinding& b : it->second )
        {
            if ( bindingDigitalDown( b, p_window, p_block ) ) return true;
        }
        return false;
    }

    bool TActionMap::pressed( const std::string& p_action, bool& p_wasDown, GLFWwindow* p_window, const BlockFn& p_block ) const
    {
        const bool raw = down( p_action, p_window, {} );  // raw ignores block so edge state stays stable
        // Re-evaluate with block for the actual hit (mirrors TInput::keyPressed).
        const bool now = down( p_action, p_window, p_block );
        const bool hit = now && !p_wasDown;
        p_wasDown      = raw;
        return hit;
    }

    float TActionMap::value( const std::string& p_action, GLFWwindow* p_window, const BlockFn& p_block ) const
    {
        const auto it = m_actions.find( p_action );
        if ( it == m_actions.end() ) return 0.0f;

        float sum = 0.0f;
        for ( const TBinding& b : it->second ) sum += bindingContribution( b, p_window, p_block );
        return std::clamp( sum, -1.0f, 1.0f );
    }
}  // namespace Tomos
