#pragma once

#include "../TEvent.hh"

namespace Tomos
{
    class TWindowResizeEvent : public TEvent
    {
    public:
        TWindowResizeEvent( int p_width, int p_height ) : m_width( p_width ), m_height( p_height ) {}

        [[nodiscard]] inline unsigned int getWidth() const { return m_width; }
        [[nodiscard]] inline unsigned int getHeight() const { return m_height; }

        [[nodiscard]] TEventType  getEventType() const override { return TEventType::WINDOW_RESIZE; }
        static TEventType         getStaticType() { return TEventType::WINDOW_RESIZE; }
        [[nodiscard]] const char* getName() const override { return "TWindowResizeEvent"; }
        [[nodiscard]] int         getCategoryFlags() const override { return static_cast<int>( TEventCategory::APPLICATION ); }
        [[nodiscard]] std::string toString() const override;

    private:
        unsigned int m_width, m_height;
    };

    class TWindowCloseEvent : public TEvent
    {
    public:
        TWindowCloseEvent() = default;

        [[nodiscard]] TEventType  getEventType() const override { return TEventType::WINDOW_CLOSE; }
        static TEventType         getStaticType() { return TEventType::WINDOW_CLOSE; }
        [[nodiscard]] const char* getName() const override { return "TWindowCloseEvent"; }
        [[nodiscard]] int         getCategoryFlags() const override { return static_cast<int>( TEventCategory::APPLICATION ); }
        [[nodiscard]] std::string toString() const override;
    };

}  // namespace Tomos
