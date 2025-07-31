#pragma once

#include "../TEvent.hh"

namespace Tomos
{
    class MouseMovedEvent : public TEvent
    {
    public:
        MouseMovedEvent( double p_x, double p_y ) : m_x( p_x ), m_y( p_y ) {}

        inline double getX() const { return m_x; }
        inline double getY() const { return m_y; }

        EventType   getEventType() const override { return EventType::MOUSE_MOVED; }
        int         getCategoryFlags() const override;
        static EventType    getStaticType() { return EventType::MOUSE_MOVED; }
        const char* getName() const override { return "MouseMovedEvent"; }
        std::string         toString() const override;

    private:
        double m_x, m_y;
    };

    class MouseScrolledEvent : public TEvent
    {
    public:
        MouseScrolledEvent( double p_xOffset, double p_yOffset ) : m_xOffset( p_xOffset ), m_yOffset( p_yOffset ) {}

        inline double getXOffset() const { return m_xOffset; }
        inline double getYOffset() const { return m_yOffset; }

        EventType   getEventType() const override { return EventType::MOUSE_SCROLLED; }
        int         getCategoryFlags() const override;
        static EventType    getStaticType() { return EventType::MOUSE_SCROLLED; }
        const char* getName() const override { return "MouseScrolledEvent"; }
        std::string         toString() const override;

    protected:
        double m_xOffset, m_yOffset;
    };

    class MouseButtonEvent : public TEvent
    {
    public:
        inline int getButton() const { return m_button; }

        int getCategoryFlags() const override;

    protected:
        explicit MouseButtonEvent( int p_button ) : m_button( p_button ) {}

        int m_button;
    };

    class MouseButtonPressedEvent : public MouseButtonEvent
    {
    public:
        explicit MouseButtonPressedEvent( int p_button ) : MouseButtonEvent( p_button ) {}

        EventType   getEventType() const override { return EventType::MOUSE_BUTTON_PRESSED; }
        static EventType    getStaticType() { return EventType::MOUSE_BUTTON_PRESSED; }
        const char* getName() const override { return "MouseButtonPressedEvent"; }
        std::string         toString() const override;
    };

    class MouseButtonReleasedEvent : public MouseButtonEvent
    {
    public:
        explicit MouseButtonReleasedEvent( int p_button ) : MouseButtonEvent( p_button ) {}

        EventType   getEventType() const override { return EventType::MOUSE_BUTTON_RELEASED; }
        static EventType    getStaticType() { return EventType::MOUSE_BUTTON_RELEASED; }
        const char* getName() const override { return "MouseButtonReleasedEvent"; }
        std::string         toString() const override;
    };

}  // namespace Tomos
