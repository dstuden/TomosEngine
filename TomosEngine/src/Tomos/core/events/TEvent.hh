#pragma once

#include <functional>
#include <string>

namespace Tomos
{
    enum class TEventType
    {
        NONE = 0,
        WINDOW_CLOSE,
        WINDOW_RESIZE,
        KEY_PRESSED,
        KEY_RELEASED,
        MOUSE_BUTTON_PRESSED,
        MOUSE_BUTTON_RELEASED,
        MOUSE_MOVED,
        MOUSE_SCROLLED
    };

    enum class TEventCategory
    {
        NONE         = 0,
        APPLICATION  = 1 << 0,
        INPUT        = 1 << 1,
        KEYBOARD     = 1 << 2,
        MOUSE        = 1 << 3,
        MOUSE_BUTTON = 1 << 4
    };

    class TEvent
    {
        friend class TEventDispatcher;

    public:
        [[nodiscard]] virtual TEventType getEventType() const = 0;

        [[nodiscard]] virtual int getCategoryFlags() const = 0;

        [[nodiscard]] virtual const char* getName() const = 0;

        [[nodiscard]] virtual std::string toString() const;

        static TEventType getStaticType() { return TEventType::NONE; }

        [[nodiscard]] bool isInCategory( TEventCategory p_category ) const { return getCategoryFlags() & static_cast<int>( p_category ); }

        [[nodiscard]] inline bool isHandled() const { return m_handled; }

        // Mark the event as consumed so lower layers do not receive it
        // (used by UI overlays when the UI captures mouse / keyboard).
        inline void setHandled( bool p_handled = true ) { m_handled = p_handled; }

    protected:
        bool       m_handled = false;
        TEventType m_type    = TEventType::NONE;
    };

    class TEventDispatcher
    {
        template<typename T>
        using TEventFn = std::function<bool( T& )>;

    public:
        explicit TEventDispatcher( TEvent& p_event ) : m_event( p_event ) {}

        template<typename T>
        bool dispatch( TEventFn<T> p_func )
        {
            if ( m_event.getEventType() == T::getStaticType() )
            {
                m_event.m_handled = p_func( *static_cast<T*>( &m_event ) );
                return true;
            }
            return false;
        }

    private:
        TEvent& m_event;
    };
}  // namespace Tomos
