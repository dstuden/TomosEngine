#pragma once

#include "../TEvent.hh"

namespace Tomos
{
    class WindowResizeEvent : public TEvent
    {
    public:
        WindowResizeEvent( int p_width, int p_height ) : m_width( p_width ), m_height( p_height ) {}

        inline unsigned int getWidth() const { return m_width; }
        inline unsigned int getHeight() const { return m_height; }

        EventType   getEventType() const override { return EventType::WINDOW_RESIZE; }
        static EventType    getStaticType() { return EventType::WINDOW_RESIZE; }
        const char* getName() const override { return "WindowResizeEvent"; }
        int         getCategoryFlags() const override { return static_cast<int>( EventCategory::APPLICATION ); }
        std::string         toString() const override;

    private:
        unsigned int m_width, m_height;
    };

    class WindowCloseEvent : public TEvent
    {
    public:
        WindowCloseEvent() = default;

        EventType   getEventType() const override { return EventType::WINDOW_CLOSE; }
        static EventType    getStaticType() { return EventType::WINDOW_CLOSE; }
        const char* getName() const override { return "WindowCloseEvent"; }
        int         getCategoryFlags() const override { return static_cast<int>( EventCategory::APPLICATION ); }
        std::string         toString() const override;
    };

    class AppTickEvent : public TEvent
    {
    public:
        AppTickEvent() = default;

        EventType   getEventType() const override { return EventType::APP_TICK; }
        static EventType    getStaticType() { return EventType::APP_TICK; }
        const char* getName() const override { return "AppTickEvent"; }
        int         getCategoryFlags() const override { return static_cast<int>( EventCategory::APPLICATION ); }
        std::string         toString() const override;
    };

    class AppUpdateEvent : public TEvent
    {
    public:
        AppUpdateEvent() = default;

        EventType   getEventType() const override { return EventType::APP_UPDATE; }
        static EventType    getStaticType() { return EventType::APP_UPDATE; }
        const char* getName() const override { return "AppUpdateEvent"; }
        int         getCategoryFlags() const override { return static_cast<int>( EventCategory::APPLICATION ); }
        std::string         toString() const override;
    };

    class AppRenderEvent : public TEvent
    {
    public:
        AppRenderEvent() = default;

        EventType   getEventType() const override { return EventType::APP_RENDER; }
        static EventType    getStaticType() { return EventType::APP_RENDER; }
        const char* getName() const override { return "AppRenderEvent"; }
        int         getCategoryFlags() const override { return static_cast<int>( EventCategory::APPLICATION ); }
        std::string         toString() const override;
    };

}  // namespace Tomos
