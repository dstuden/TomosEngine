#pragma once

#include "../TEvent.hh"

namespace Tomos
{
    class TKeyEvent : public TEvent
    {
    public:
        inline int getKeyCode() const { return m_keyCode; }
        int        getCategoryFlags() const override;

    protected:
        explicit TKeyEvent( int p_keyCode ) : m_keyCode( p_keyCode ) {}

        int m_keyCode;
    };

    class TKeyPressedEvent : public TKeyEvent
    {
    public:
        TKeyPressedEvent( int p_keyCode, int p_repeatCount ) : TKeyEvent( p_keyCode ), m_repeatCount( p_repeatCount ) {}

        inline int getRepeatCount() const { return m_repeatCount; }

        TEventType        getEventType() const override { return TEventType::KEY_PRESSED; }
        static TEventType getStaticType() { return TEventType::KEY_PRESSED; }
        const char*       getName() const override { return "TKeyPressedEvent"; }
        std::string       toString() const override;

    protected:
        int m_repeatCount;
    };

    class TKeyReleasedEvent : public TKeyEvent
    {
    public:
        explicit TKeyReleasedEvent( int p_keyCode ) : TKeyEvent( p_keyCode ) {}

        TEventType        getEventType() const override { return TEventType::KEY_RELEASED; }
        static TEventType getStaticType() { return TEventType::KEY_RELEASED; }
        const char*       getName() const override { return "TKeyReleasedEvent"; }
        std::string       toString() const override;
    };

}  // namespace Tomos
