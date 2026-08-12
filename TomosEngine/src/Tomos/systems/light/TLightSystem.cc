#include "Tomos/systems/light/TLightSystem.hh"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "Tomos/core/scene/TSceneNode.hh"
#include "Tomos/gpu/vulkan/TVkGpu.hh"
#include "Tomos/gpu/vulkan/TVkPass.hh"
#include "Tomos/util/logger/TLogger.hh"
#include "Tomos/util/math/TPointShadow.hh"
#include "Tomos/util/math/TProjection.hh"

namespace Tomos
{
    void TLightSystem::componentCreated( TSceneNode& p_node, TComponent& p_component )
    {
        auto& lc        = static_cast<TLightComponent&>( p_component );
        m_lights[ &lc ] = &p_node;
    }

    void TLightSystem::componentDestroyed( TSceneNode& p_node, TComponent& p_component ) { m_lights.erase( &static_cast<TLightComponent&>( p_component ) ); }

    void TLightSystem::populate( TFrameState& p_state ) const
    {
        p_state.m_lights.clear();

        constexpr float k_dirShadowHalfSize = 40.0f;
        constexpr float k_dirShadowDepth    = 120.0f;

        uint32_t nextShadowSlot   = 0;
        bool     truncatedLights  = false;
        bool     truncatedShadows = false;

        for ( const auto& [ lc, node ] : m_lights )
        {
            if ( p_state.m_lights.size() >= k_maxLights )
            {
                truncatedLights = true;
                break;
            }

            const glm::mat4& world = node->m_transform.getGlobalMatrix();

            TLightData data{};
            data.m_position  = glm::vec3( world[ 3 ] );
            data.m_color     = lc->m_color;
            data.m_intensity = lc->m_intensity;
            data.m_maxRange  = lc->m_maxRange;
            data.m_innerCone = glm::cos( lc->m_innerCone );
            data.m_outerCone = glm::cos( lc->m_outerCone );
            data.m_shadowMap = -1;
            data.m_vp        = glm::mat4( 1.0f );

            switch ( lc->m_type )
            {
                case TLightType::Point:
                    data.m_type      = 0;
                    data.m_direction = glm::vec3( 0.0f, -1.0f, 0.0f );
                    break;

                case TLightType::Directional:
                    data.m_type = 1;
                    data.m_direction = glm::normalize( glm::vec3( world * glm::vec4( 0.0f, 0.0f, -1.0f, 0.0f ) ) );
                    break;

                case TLightType::Spot:
                    data.m_type      = 2;
                    data.m_direction = glm::normalize( glm::vec3( world * glm::vec4( 0.0f, 0.0f, -1.0f, 0.0f ) ) );
                    break;
            }

            // Assign shadow-map array layer(s).  Point lights take six consecutive
            // faces; spot / directional take one.  Face VPs are rebuilt in the
            // shadow pass / forward shader (see TPointShadow.hh).
            if ( lc->m_castShadow )
            {
                const uint32_t slotsNeeded = ( lc->m_type == TLightType::Point ) ? k_pointShadowFaces : 1u;
                if ( nextShadowSlot + slotsNeeded <= k_maxShadowMaps )
                {
                    data.m_shadowMap = static_cast<int32_t>( nextShadowSlot );
                    nextShadowSlot += slotsNeeded;

                    if ( lc->m_type != TLightType::Point )
                    {
                        // Avoid a degenerate view matrix when the light points straight up/down.
                        const glm::vec3 up        = glm::abs( data.m_direction.y ) > 0.99f ? glm::vec3( 0.0f, 0.0f, 1.0f ) : glm::vec3( 0.0f, 1.0f, 0.0f );
                        const glm::mat4 lightView = glm::lookAt( data.m_position, data.m_position + data.m_direction, up );

                        glm::mat4 lightProj;
                        if ( lc->m_type == TLightType::Directional )
                        {
                            lightProj = orthoVk( -k_dirShadowHalfSize, k_dirShadowHalfSize, -k_dirShadowHalfSize, k_dirShadowHalfSize, 0.1f, k_dirShadowDepth );
                        }
                        else  // Spot — perspective frustum matching the outer cone.
                        {
                            lightProj = perspectiveVk( 2.0f * lc->m_outerCone, 1.0f, 0.1f, glm::max( lc->m_maxRange, 1.0f ) );
                        }

                        data.m_vp = lightProj * lightView;
                    }
                }
                else
                {
                    truncatedShadows = true;
                }
            }

            p_state.m_lights.push_back( data );
        }

        if ( truncatedLights ) TLOG_WARN() << "[TLightSystem] Light cap reached (" << k_maxLights << ") — dropping remaining lights";
        if ( truncatedShadows ) TLOG_WARN() << "[TLightSystem] Shadow map slots full (" << k_maxShadowMaps << ") — extra casters have no shadow";
    }
}  // namespace Tomos
