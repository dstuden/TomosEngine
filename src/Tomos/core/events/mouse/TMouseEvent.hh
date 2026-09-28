#pragma once

#include "../TEvent.hh"

namespace Tomos
{
    class TMouseMovedEvent : public TEvent
    {
    public:
        TMouseMovedEvent( double p_x, double p_y ) : m_x( p_x ), m_y( p_y ) {}

        [[nodiscard]] inline double getX() const { return m_x; }
        [[nodiscard]] inline double getY() const { return m_y; }

        [[nodiscard]] TEventType getEventType() const override { return TEventType::MOUSE_MOVED; }
        [[nodiscard]] int        getCategoryFlags() const override;

        static TEventType         getStaticType() { return TEventType::MOUSE_MOVED; }
        [[nodiscard]] const char* getName() const override { return "TMouseMovedEvent"; }
        [[nodiscard]] std::string toString() const override;

    private:
        double m_x, m_y;
    };

    class TMouseScrolledEvent : public TEvent
    {
    public:
        TMouseScrolledEvent( double p_xOffset, double p_yOffset ) : m_xOffset( p_xOffset ), m_yOffset( p_yOffset ) {}

        [[nodiscard]] inline double getXOffset() const { return m_xOffset; }
        [[nodiscard]] inline double getYOffset() const { return m_yOffset; }

        [[nodiscard]] TEventType getEventType() const override { return TEventType::MOUSE_SCROLLED; }
        [[nodiscard]] int        getCategoryFlags() const override;

        static TEventType         getStaticType() { return TEventType::MOUSE_SCROLLED; }
        [[nodiscard]] const char* getName() const override { return "TMouseScrolledEvent"; }
        [[nodiscard]] std::string toString() const override;

    protected:
        double m_xOffset, m_yOffset;
    };

    class TMouseButtonEvent : public TEvent
    {
    public:
        [[nodiscard]] inline int getButton() const { return m_button; }

        [[nodiscard]] int getCategoryFlags() const override;

    protected:
        explicit TMouseButtonEvent( int p_button ) : m_button( p_button ) {}

        int m_button;
    };

    class TMouseButtonPressedEvent : public TMouseButtonEvent
    {
    public:
        explicit TMouseButtonPressedEvent( int p_button ) : TMouseButtonEvent( p_button ) {}

        [[nodiscard]] TEventType  getEventType() const override { return TEventType::MOUSE_BUTTON_PRESSED; }
        static TEventType         getStaticType() { return TEventType::MOUSE_BUTTON_PRESSED; }
        [[nodiscard]] const char* getName() const override { return "TMouseButtonPressedEvent"; }
        [[nodiscard]] std::string toString() const override;
    };

    class TMouseButtonReleasedEvent : public TMouseButtonEvent
    {
    public:
        explicit TMouseButtonReleasedEvent( int p_button ) : TMouseButtonEvent( p_button ) {}

        [[nodiscard]] TEventType  getEventType() const override { return TEventType::MOUSE_BUTTON_RELEASED; }
        static TEventType         getStaticType() { return TEventType::MOUSE_BUTTON_RELEASED; }
        [[nodiscard]] const char* getName() const override { return "TMouseButtonReleasedEvent"; }
        [[nodiscard]] std::string toString() const override;
    };

}  // namespace Tomos
