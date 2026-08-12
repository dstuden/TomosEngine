#pragma once

#include <glm/glm.hpp>
#include <limits>

namespace Tomos
{
    struct TAABB
    {
        glm::vec3 m_min{ 0.0f };
        glm::vec3 m_max{ 0.0f };

        [[nodiscard]] bool valid() const { return m_min.x <= m_max.x; }

        [[nodiscard]] glm::vec3 center() const { return 0.5f * ( m_min + m_max ); }

        [[nodiscard]] TAABB transformed( const glm::mat4& p_m ) const
        {
            const glm::vec3 corners[ 8 ] = {
                    { m_min.x, m_min.y, m_min.z }, { m_max.x, m_min.y, m_min.z }, { m_min.x, m_max.y, m_min.z }, { m_max.x, m_max.y, m_min.z },
                    { m_min.x, m_min.y, m_max.z }, { m_max.x, m_min.y, m_max.z }, { m_min.x, m_max.y, m_max.z }, { m_max.x, m_max.y, m_max.z },
            };

            TAABB out;
            out.m_min = glm::vec3( std::numeric_limits<float>::max() );
            out.m_max = glm::vec3( std::numeric_limits<float>::lowest() );
            for ( const glm::vec3& c : corners )
            {
                const glm::vec3 w = glm::vec3( p_m * glm::vec4( c, 1.0f ) );
                out.m_min         = glm::min( out.m_min, w );
                out.m_max         = glm::max( out.m_max, w );
            }
            return out;
        }
    };

    // Vulkan / GLM_FORCE_DEPTH_ZERO_TO_ONE: near plane is row 2 alone (not row3 + row2).
    inline void extractFrustumPlanes( const glm::mat4& p_viewProj, glm::vec4 p_planes[ 6 ] )
    {
        const glm::mat4& m = p_viewProj;

        p_planes[ 0 ] = glm::vec4( m[ 0 ][ 3 ] + m[ 0 ][ 0 ], m[ 1 ][ 3 ] + m[ 1 ][ 0 ], m[ 2 ][ 3 ] + m[ 2 ][ 0 ],
                                   m[ 3 ][ 3 ] + m[ 3 ][ 0 ] );  // left
        p_planes[ 1 ] = glm::vec4( m[ 0 ][ 3 ] - m[ 0 ][ 0 ], m[ 1 ][ 3 ] - m[ 1 ][ 0 ], m[ 2 ][ 3 ] - m[ 2 ][ 0 ],
                                   m[ 3 ][ 3 ] - m[ 3 ][ 0 ] );  // right
        p_planes[ 2 ] = glm::vec4( m[ 0 ][ 3 ] + m[ 0 ][ 1 ], m[ 1 ][ 3 ] + m[ 1 ][ 1 ], m[ 2 ][ 3 ] + m[ 2 ][ 1 ],
                                   m[ 3 ][ 3 ] + m[ 3 ][ 1 ] );  // bottom
        p_planes[ 3 ] = glm::vec4( m[ 0 ][ 3 ] - m[ 0 ][ 1 ], m[ 1 ][ 3 ] - m[ 1 ][ 1 ], m[ 2 ][ 3 ] - m[ 2 ][ 1 ],
                                   m[ 3 ][ 3 ] - m[ 3 ][ 1 ] );  // top
        p_planes[ 4 ] = glm::vec4( m[ 0 ][ 2 ], m[ 1 ][ 2 ], m[ 2 ][ 2 ],
                                   m[ 3 ][ 2 ] );  // near (z' ≥ 0)
        p_planes[ 5 ] = glm::vec4( m[ 0 ][ 3 ] - m[ 0 ][ 2 ], m[ 1 ][ 3 ] - m[ 1 ][ 2 ], m[ 2 ][ 3 ] - m[ 2 ][ 2 ],
                                   m[ 3 ][ 3 ] - m[ 3 ][ 2 ] );  // far

        for ( int i = 0; i < 6; ++i )
        {
            const float len = glm::length( glm::vec3( p_planes[ i ] ) );
            if ( len > 1e-6f ) p_planes[ i ] /= len;
        }
    }

    inline bool aabbIntersectsFrustum( const TAABB& p_box, const glm::vec4 p_planes[ 6 ] )
    {
        for ( int i = 0; i < 6; ++i )
        {
            const glm::vec4& pl = p_planes[ i ];
            const glm::vec3  p{ pl.x >= 0.0f ? p_box.m_max.x : p_box.m_min.x, pl.y >= 0.0f ? p_box.m_max.y : p_box.m_min.y,
                                pl.z >= 0.0f ? p_box.m_max.z : p_box.m_min.z };
            if ( glm::dot( glm::vec3( pl ), p ) + pl.w < 0.0f ) return false;
        }
        return true;
    }
}  // namespace Tomos
