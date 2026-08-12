#include "TApplicationEvent.hh"

#include <sstream>

namespace Tomos
{
    std::string TWindowResizeEvent::toString() const
    {
        std::stringstream ss;
        ss << "TWindowResizeEvent: " << m_width << ", " << m_height;
        return ss.str();
    }

    std::string TWindowCloseEvent::toString() const { return getName(); }
}  // namespace Tomos
