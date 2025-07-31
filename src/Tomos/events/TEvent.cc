//
// Created by dstuden on 1/19/25.
//

#include <sstream>

#include "TEvent.hh"

namespace Tomos
{
    std::string TEvent::toString() const
    {
        std::stringstream ss;
        ss << getName() << " ( " << static_cast<int>( m_type ) << " )";
        return ss.str();
    }

    bool TEvent::isInCategory( EventCategory p_category ) const
    {
        return getCategoryFlags() & static_cast<int>( p_category );
    }

}  // namespace Tomos
