#include "TKeyEvent.hh"

#include <sstream>

namespace Tomos
{
    int TKeyEvent::getCategoryFlags() const { return static_cast<int>( TEventCategory::KEYBOARD ) | static_cast<int>( TEventCategory::INPUT ); }

    std::string TKeyPressedEvent::toString() const
    {
        std::stringstream ss;
        ss << "TKeyPressedEvent: " << m_keyCode << " ( " << m_repeatCount << " repeats )";
        return ss.str();
    }

    std::string TKeyReleasedEvent::toString() const
    {
        std::stringstream ss;
        ss << "TKeyReleasedEvent: " << m_keyCode;
        return ss.str();
    }
}  // namespace Tomos
