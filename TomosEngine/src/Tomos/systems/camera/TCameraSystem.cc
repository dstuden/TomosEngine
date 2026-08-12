#include "Tomos/systems/camera/TCameraSystem.hh"

#include <cstdint>
#include <glm/gtc/matrix_transform.hpp>
#include <limits>

#include "Tomos/core/scene/TSceneNode.hh"
#include "Tomos/gpu/vulkan/TVkPass.hh"
#include "Tomos/util/logger/TLogger.hh"

namespace Tomos
{
    void TCameraSystem::componentCreated( TSceneNode& p_node, TComponent& p_component )
    {
        auto& cc         = static_cast<TCameraComponent&>( p_component );
        m_cameras[ &cc ] = &p_node;
    }

    void TCameraSystem::componentDestroyed( TSceneNode& /*p_node*/, TComponent& p_component )
    {
        auto& cc = static_cast<TCameraComponent&>( p_component );
        m_cameras.erase( &cc );
        if ( m_active == &cc )
        {
            m_active     = nullptr;
            m_activeNode = nullptr;
        }
    }

    void TCameraSystem::lateUpdate( float /*p_dt*/ )
    {
        m_active     = nullptr;
        m_activeNode = nullptr;

        TCameraComponent* bestCc   = nullptr;
        TSceneNode*       bestNode = nullptr;
        uint64_t          bestId   = std::numeric_limits<uint64_t>::max();
        int               actives  = 0;

        for ( auto& [ cc, node ] : m_cameras )
        {
            if ( !cc->m_active || node == nullptr ) continue;
            ++actives;
            if ( node->m_id < bestId )
            {
                bestId   = node->m_id;
                bestCc   = cc;
                bestNode = node;
            }
        }

        if ( actives > 1 )
        {
            TLOG_WARN() << "[TCameraSystem] " << actives << " cameras marked active — using lowest node id " << bestId << " ('" << bestNode->m_name << "')";
        }

        m_active     = bestCc;
        m_activeNode = bestNode;
    }

    void TCameraSystem::populate( TFrameState& p_state, float p_aspect ) const
    {
        if ( m_active == nullptr || m_activeNode == nullptr ) return;

        p_state.m_view        = m_activeNode->m_transform.getGlobalInvMatrix();
        p_state.m_viewInv     = m_activeNode->m_transform.getGlobalMatrix();
        p_state.m_proj        = m_active->projMatrix( p_aspect );
        p_state.m_projInv     = m_active->projMatrixInv( p_aspect );
        p_state.m_viewProj    = p_state.m_proj * p_state.m_view;
        p_state.m_viewProjInv = glm::inverse( p_state.m_viewProj );
        p_state.m_near        = m_active->m_near;
        p_state.m_far         = m_active->m_far;
    }
}  // namespace Tomos
