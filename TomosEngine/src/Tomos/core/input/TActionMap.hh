#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "Tomos/core/input/TInput.hh"

struct GLFWwindow;

namespace Tomos
{
    // Coarse input layers. Bindings tagged Any always apply; otherwise a binding
    // is live only when it matches the map's active context.
    enum class TInputContext
    {
        Gameplay,
        UI,
        Any,
    };

    // Digital: any binding OR'd. Analog: contributions summed, clamped [-1, 1].
    class TActionMap
    {
    public:
        using BlockFn = TInput::BlockFn;

        void                    setActiveContext( TInputContext p_context );
        [[nodiscard]] TInputContext activeContext() const { return m_active; }

        void clear();

        void bindKey( const std::string& p_action, int p_key, TInputContext p_context = TInputContext::Gameplay, float p_scale = 1.0f );
        void bindMouseButton( const std::string& p_action, int p_button, TInputContext p_context = TInputContext::Gameplay, float p_scale = 1.0f );
        void bindGamepadButton( const std::string& p_action, int p_button, int p_jid = 0, TInputContext p_context = TInputContext::Gameplay,
                                float p_scale = 1.0f );
        void bindGamepadAxis( const std::string& p_action, int p_axis, float p_scale = 1.0f, float p_deadzone = 0.15f, int p_jid = 0,
                              TInputContext p_context = TInputContext::Gameplay );

        void bindKeyAxis( const std::string& p_action, int p_negativeKey, int p_positiveKey, TInputContext p_context = TInputContext::Gameplay );

        [[nodiscard]] bool  down( const std::string& p_action, GLFWwindow* p_window = nullptr, const BlockFn& p_block = {} ) const;
        [[nodiscard]] bool  pressed( const std::string& p_action, bool& p_wasDown, GLFWwindow* p_window = nullptr, const BlockFn& p_block = {} ) const;
        [[nodiscard]] float value( const std::string& p_action, GLFWwindow* p_window = nullptr, const BlockFn& p_block = {} ) const;

    private:
        enum class TKind
        {
            Key,
            MouseButton,
            GamepadButton,
            GamepadAxis,
        };

        struct TBinding
        {
            TKind         m_kind{};
            int           m_code{};
            float         m_scale{ 1.0f };
            float         m_deadzone{ 0.15f };
            int           m_jid{ 0 };
            TInputContext m_context{ TInputContext::Gameplay };
        };

        [[nodiscard]] bool bindingActive( const TBinding& p_binding ) const;
        [[nodiscard]] bool bindingDigitalDown( const TBinding& p_binding, GLFWwindow* p_window, const BlockFn& p_block ) const;
        [[nodiscard]] float bindingContribution( const TBinding& p_binding, GLFWwindow* p_window, const BlockFn& p_block ) const;

        TInputContext                                     m_active{ TInputContext::Gameplay };
        std::unordered_map<std::string, std::vector<TBinding>> m_actions;
    };
}  // namespace Tomos
