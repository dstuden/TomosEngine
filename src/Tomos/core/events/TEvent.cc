#include "TEvent.hh"

#include <sstream>

namespace Tomos
{
    std::string TEvent::toString() const
    {
        std::stringstream ss;
        ss << getName() << " ( " << static_cast<int>( m_type ) << " )";
        return ss.str();
    }

}  // namespace Tomos
