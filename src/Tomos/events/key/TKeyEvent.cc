//
// Created by dstuden on 1/19/25.
//

#include <sstream>

#include "TKeyEvent.hh"

namespace Tomos
{
    int TKeyEvent::getCategoryFlags() const
    {
        return static_cast<int>( EventCategory::KEYBOARD ) | static_cast<int>( EventCategory::INPUT );
    }

    std::string KeyPressedEvent::toString() const
    {
        std::stringstream ss;
        ss << "KeyPressedEvent: " << m_keyCode << " ( " << m_repeatCount << " repeats )";
        return ss.str();
    }

    std::string KeyReleasedEvent::toString() const
    {
        std::stringstream ss;
        ss << "KeyReleasedEvent: " << m_keyCode;
        return ss.str();
    }
} // namespace Tomos
