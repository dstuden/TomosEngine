#pragma once

#include "../TEvent.hh"

namespace Tomos
{
    class TMouseMovedEvent : public TEvent
    {
    public:
        TMouseMovedEvent( double p_x, double p_y ) : m_x( p_x ), m_y( p_y ) {}

        inline double getX() const { return m_x; }
        inline double getY() const { return m_y; }

        TEventType getEventType() const override { return TEventType::MOUSE_MOVED; }
        int        getCategoryFlags() const override;

        static TEventType getStaticType() { return TEventType::MOUSE_MOVED; }
        const char*       getName() const override { return "TMouseMovedEvent"; }
        std::string       toString() const override;

    private:
        double m_x, m_y;
    };

    class TMouseScrolledEvent : public TEvent
    {
    public:
        TMouseScrolledEvent( double p_xOffset, double p_yOffset ) : m_xOffset( p_xOffset ), m_yOffset( p_yOffset ) {}

        inline double getXOffset() const { return m_xOffset; }
        inline double getYOffset() const { return m_yOffset; }

        TEventType getEventType() const override { return TEventType::MOUSE_SCROLLED; }
        int        getCategoryFlags() const override;

        static TEventType getStaticType() { return TEventType::MOUSE_SCROLLED; }
        const char*       getName() const override { return "TMouseScrolledEvent"; }
        std::string       toString() const override;

    protected:
        double m_xOffset, m_yOffset;
    };

    class TMouseButtonEvent : public TEvent
    {
    public:
        inline int getButton() const { return m_button; }

        int getCategoryFlags() const override;

    protected:
        explicit TMouseButtonEvent( int p_button ) : m_button( p_button ) {}

        int m_button;
    };

    class TMouseButtonPressedEvent : public TMouseButtonEvent
    {
    public:
        explicit TMouseButtonPressedEvent( int p_button ) : TMouseButtonEvent( p_button ) {}

        TEventType        getEventType() const override { return TEventType::MOUSE_BUTTON_PRESSED; }
        static TEventType getStaticType() { return TEventType::MOUSE_BUTTON_PRESSED; }
        const char*       getName() const override { return "TMouseButtonPressedEvent"; }
        std::string       toString() const override;
    };

    class TMouseButtonReleasedEvent : public TMouseButtonEvent
    {
    public:
        explicit TMouseButtonReleasedEvent( int p_button ) : TMouseButtonEvent( p_button ) {}

        TEventType        getEventType() const override { return TEventType::MOUSE_BUTTON_RELEASED; }
        static TEventType getStaticType() { return TEventType::MOUSE_BUTTON_RELEASED; }
        const char*       getName() const override { return "TMouseButtonReleasedEvent"; }
        std::string       toString() const override;
    };

}  // namespace Tomos
