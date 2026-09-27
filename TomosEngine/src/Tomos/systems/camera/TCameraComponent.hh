#pragma once

#include <glm/glm.hpp>

#include "Tomos/systems/TComponent.hh"
#include "Tomos/util/math/TProjection.hh"
#include "Tomos/util/reflect/TReflectAttr.hh"

namespace Tomos
{
    enum class TProjection
    {
        Perspective,
        Orthographic
    };

    // m_fov vertical radians (perspective); m_orthoHalfHeight world units (ortho).
    class TOMOS_ANN( Reflect::ComponentMeta{ "camera", "Camera" } ) TCameraComponent : public TComponent
    {
    public:
        bool        m_active     = true;
        TProjection m_projection = TProjection::Perspective;
        TOMOS_ANN( Reflect::Degrees{} )
        TOMOS_ANN( Reflect::UiRange{ 10.0f, 120.0f } ) TOMOS_ANN( Reflect::UiLabel{ "FOV (deg)" } ) float m_fov = glm::radians( 60.0f );
        float m_near                                                                                            = 0.1f;
        float m_far                                                                                             = 1000.0f;
        TOMOS_ANN( Reflect::UiLabel{ "Ortho half-height" } ) float m_orthoHalfHeight                            = 5.0f;

        [[nodiscard]] const glm::mat4& projMatrix( float p_aspect )
        {
            if ( p_aspect != m_cachedAspect || m_dirty ) recompute( p_aspect );
            return m_projMtx;
        }

        [[nodiscard]] const glm::mat4& projMatrixInv( float p_aspect )
        {
            if ( p_aspect != m_cachedAspect || m_dirty ) recompute( p_aspect );
            return m_projMtxInv;
        }

        void setDirty() { m_dirty = true; }

    private:
        void recompute( float p_aspect )
        {
            if ( m_projection == TProjection::Perspective )
            {
                m_projMtx = perspectiveVk( m_fov, p_aspect, m_near, m_far );
            }
            else
            {
                const float hw = m_orthoHalfHeight * p_aspect;
                m_projMtx      = orthoVk( -hw, hw, -m_orthoHalfHeight, m_orthoHalfHeight, m_near, m_far );
            }

            m_projMtxInv   = glm::inverse( m_projMtx );
            m_cachedAspect = p_aspect;
            m_dirty        = false;
        }

        glm::mat4 m_projMtx{ 1.0f };
        glm::mat4 m_projMtxInv{ 1.0f };
        float     m_cachedAspect = 0.0f;
        bool      m_dirty        = true;
    };
}  // namespace Tomos
