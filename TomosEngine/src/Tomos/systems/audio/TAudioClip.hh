#pragma once

#include <string>

namespace Tomos
{
    // TAudioComponent borrows — keep alive while emitters use it.
    class TAudioClip
    {
    public:
        explicit TAudioClip( std::string p_path, std::string p_name = {} ) : m_path( std::move( p_path ) ), m_name( std::move( p_name ) )
        {
            if ( m_name.empty() ) m_name = m_path;
        }

        std::string m_path;
        std::string m_name;
    };
}  // namespace Tomos
