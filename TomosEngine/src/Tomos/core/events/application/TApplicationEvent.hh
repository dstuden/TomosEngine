#pragma once

#include "../TEvent.hh"

namespace Tomos
{
    class TWindowResizeEvent : public TEvent
    {
    public:
        TWindowResizeEvent( int p_width, int p_height ) : m_width( p_width ), m_height( p_height ) {}

        inline unsigned int getWidth() const { return m_width; }
        inline unsigned int getHeight() const { return m_height; }

        TEventType        getEventType() const override { return TEventType::WINDOW_RESIZE; }
        static TEventType getStaticType() { return TEventType::WINDOW_RESIZE; }
        const char*       getName() const override { return "TWindowResizeEvent"; }
        int               getCategoryFlags() const override { return static_cast<int>( TEventCategory::APPLICATION ); }
        std::string       toString() const override;

    private:
        unsigned int m_width, m_height;
    };

    class TWindowCloseEvent : public TEvent
    {
    public:
        TWindowCloseEvent() = default;

        TEventType        getEventType() const override { return TEventType::WINDOW_CLOSE; }
        static TEventType getStaticType() { return TEventType::WINDOW_CLOSE; }
        const char*       getName() const override { return "TWindowCloseEvent"; }
        int               getCategoryFlags() const override { return static_cast<int>( TEventCategory::APPLICATION ); }
        std::string       toString() const override;
    };

}  // namespace Tomos
