#include "TMouseEvent.hh"

#include <sstream>

namespace Tomos
{

    int TMouseMovedEvent::getCategoryFlags() const { return static_cast<int>( TEventCategory::MOUSE ) | static_cast<int>( TEventCategory::INPUT ); }

    std::string TMouseMovedEvent::toString() const
    {
        std::stringstream ss;
        ss << "TMouseMovedEvent: " << m_x << ", " << m_y;
        return ss.str();
    }

    int TMouseScrolledEvent::getCategoryFlags() const { return static_cast<int>( TEventCategory::MOUSE ) | static_cast<int>( TEventCategory::INPUT ); }

    std::string TMouseScrolledEvent::toString() const
    {
        std::stringstream ss;
        ss << "TMouseScrolledEvent: " << m_xOffset << ", " << m_yOffset;
        return ss.str();
    }

    int TMouseButtonEvent::getCategoryFlags() const
    {
        return static_cast<int>( TEventCategory::MOUSE ) | static_cast<int>( TEventCategory::INPUT ) | static_cast<int>( TEventCategory::MOUSE_BUTTON );
    }

    std::string TMouseButtonPressedEvent::toString() const
    {
        std::stringstream ss;
        ss << "TMouseButtonPressedEvent: " << m_button;
        return ss.str();
    }

    std::string TMouseButtonReleasedEvent::toString() const
    {
        std::stringstream ss;
        ss << "TMouseButtonReleasedEvent: " << m_button;
        return ss.str();
    }

}  // namespace Tomos
