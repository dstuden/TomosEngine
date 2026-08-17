#include "Tomos/ui/editor/TSceneViewportPanel.hh"

#include <ImGuizmo.h>
#include <algorithm>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <imgui.h>
#include <limits>
#include <vector>

#include "Tomos/core/app/TApplication.hh"
#include "Tomos/core/scene/TSceneNode.hh"
#include "Tomos/gpu/vulkan/TVkGpu.hh"
#include "Tomos/gpu/vulkan/TVkMesh.hh"
#include "Tomos/gpu/vulkan/TVkPass.hh"
#include "Tomos/systems/mesh/TMeshComponent.hh"
#include "Tomos/systems/physics/TColliderComponent.hh"
#include "Tomos/systems/physics/TPhysicsSystem.hh"
#include "Tomos/systems/physics/TRigidBodyComponent.hh"
#include "Tomos/ui/editor/TDebugDraw.hh"
#include "Tomos/util/math/TFrustum.hh"

namespace Tomos
{
    namespace
    {
        void expandAabb( TAABB& p_dst, const TAABB& p_src )
        {
            if ( !p_src.valid() ) return;
            if ( !p_dst.valid() || p_dst.m_min.x > p_dst.m_max.x )
            {
                p_dst = p_src;
                return;
            }
            p_dst.m_min = glm::min( p_dst.m_min, p_src.m_min );
            p_dst.m_max = glm::max( p_dst.m_max, p_src.m_max );
        }

        void collectSelectionAabb( TSceneNode& p_node, TAABB& p_out, bool& p_any )
        {
            if ( const auto* mesh = p_node.findComponent<TMeshComponent>() )
            {
                if ( mesh->m_mesh != nullptr && mesh->m_mesh->m_aabb.valid() )
                {
                    expandAabb( p_out, mesh->m_mesh->m_aabb.transformed( p_node.worldMatrix() ) );
                    p_any = true;
                }
            }

            if ( const auto* col = p_node.findComponent<TColliderComponent>() )
            {
                if ( col->m_enabled )
                {
                    glm::vec3 mn, mx;
                    if ( TPhysicsSystem::colliderWorldAabb( p_node, *col, mn, mx ) )
                    {
                        TAABB box;
                        box.m_min = mn;
                        box.m_max = mx;
                        expandAabb( p_out, box );
                        p_any = true;
                    }
                }
            }

            for ( const auto& child : p_node.getChildren() )
            {
                if ( child ) collectSelectionAabb( *child, p_out, p_any );
            }
        }

        bool rayAabb( const glm::vec3& p_origin, const glm::vec3& p_dir, const TAABB& p_box, float& p_t )
        {
            float tmin = 0.0f;
            float tmax = std::numeric_limits<float>::max();
            for ( int i = 0; i < 3; ++i )
            {
                const float o    = p_origin[ i ];
                const float d    = p_dir[ i ];
                const float bmin = p_box.m_min[ i ];
                const float bmax = p_box.m_max[ i ];
                if ( std::abs( d ) < 1e-8f )
                {
                    if ( o < bmin || o > bmax ) return false;
                    continue;
                }
                float t1 = ( bmin - o ) / d;
                float t2 = ( bmax - o ) / d;
                if ( t1 > t2 ) std::swap( t1, t2 );
                tmin = std::max( tmin, t1 );
                tmax = std::min( tmax, t2 );
                if ( tmin > tmax ) return false;
            }
            p_t = tmin;
            return tmax >= 0.0f;
        }

        void gatherPickable( TSceneNode& p_node, std::vector<std::pair<TSceneNode*, TAABB>>& p_out )
        {
            TAABB box;
            box.m_min = glm::vec3( std::numeric_limits<float>::max() );
            box.m_max = glm::vec3( std::numeric_limits<float>::lowest() );
            bool any  = false;
            collectSelectionAabb( p_node, box, any );
            if ( !any )
            {
                const glm::vec3 c( p_node.worldMatrix()[ 3 ] );
                constexpr float kHalf = 0.25f;
                box.m_min             = c - glm::vec3( kHalf );
                box.m_max             = c + glm::vec3( kHalf );
            }
            p_out.emplace_back( &p_node, box );
            for ( const auto& child : p_node.getChildren() )
                if ( child ) gatherPickable( *child, p_out );
        }

        void pickNode( TSceneEditorContext& p_ctx, const TFrameState& p_state, const ImVec2& p_mouse )
        {
            if ( p_ctx.m_scene == nullptr || p_ctx.m_viewportSize.x <= 1.0f || p_ctx.m_viewportSize.y <= 1.0f ) return;

            const float u = ( p_mouse.x - p_ctx.m_viewportPos.x ) / p_ctx.m_viewportSize.x;
            const float v = ( p_mouse.y - p_ctx.m_viewportPos.y ) / p_ctx.m_viewportSize.y;
            if ( u < 0.0f || u > 1.0f || v < 0.0f || v > 1.0f )
            {
                p_ctx.clearSelection();
                return;
            }

            const float ndcX = u * 2.0f - 1.0f;
            const float ndcY = v * 2.0f - 1.0f;

            const glm::vec4 nearH = p_state.m_viewProjInv * glm::vec4( ndcX, ndcY, 0.0f, 1.0f );
            const glm::vec4 farH  = p_state.m_viewProjInv * glm::vec4( ndcX, ndcY, 1.0f, 1.0f );
            const glm::vec3 nearP = glm::vec3( nearH ) / nearH.w;
            const glm::vec3 farP  = glm::vec3( farH ) / farH.w;
            const glm::vec3 dir   = glm::normalize( farP - nearP );

            std::vector<std::pair<TSceneNode*, TAABB>> candidates;
            for ( const auto& root : p_ctx.m_scene->getChildren() )
                if ( root ) gatherPickable( *root, candidates );

            float       bestT    = std::numeric_limits<float>::max();
            TSceneNode* bestNode = nullptr;
            for ( const auto& [ node, box ] : candidates )
            {
                float t = 0.0f;
                if ( rayAabb( nearP, dir, box, t ) && t >= 0.0f && t < bestT )
                {
                    bestT    = t;
                    bestNode = node;
                }
            }

            if ( bestNode != nullptr )
                p_ctx.select( bestNode->m_id );
            else
                p_ctx.clearSelection();
        }

        void drawOverlays( TSceneEditorContext& p_ctx, const TFrameState& p_state, ImDrawList* p_dl )
        {
            if ( p_dl == nullptr ) return;
            const ImVec2 pos  = p_ctx.m_viewportPos;
            const ImVec2 size = p_ctx.m_viewportSize;

            if ( p_ctx.m_overlaySelection )
            {
                if ( TSceneNode* node = p_ctx.selectedNode() )
                {
                    TAABB box;
                    box.m_min = glm::vec3( std::numeric_limits<float>::max() );
                    box.m_max = glm::vec3( std::numeric_limits<float>::lowest() );
                    bool any  = false;
                    collectSelectionAabb( *node, box, any );
                    if ( !any )
                    {
                        const glm::vec3 c( node->worldMatrix()[ 3 ] );
                        constexpr float kHalf = 0.25f;
                        box.m_min             = c - glm::vec3( kHalf );
                        box.m_max             = c + glm::vec3( kHalf );
                    }
                    TDebugDraw::aabb( p_dl, p_state.m_viewProj, pos, size, box, IM_COL32( 0, 220, 255, 220 ), 2.0f );
                }
            }

            if ( p_ctx.m_scene == nullptr ) return;
            auto* phys = p_ctx.m_scene->ecs().maybeGetSystem<TPhysicsSystem>();
            if ( phys == nullptr ) return;

            if ( p_ctx.m_overlayColliders )
            {
                phys->forEachCollider(
                        [ & ]( const TPhysicsSystem::TColliderDebug& p_c )
                        {
                            if ( p_c.m_node == nullptr || p_c.m_collider == nullptr ) return;
                            glm::vec3 mn, mx;
                            if ( !TPhysicsSystem::colliderWorldAabb( *p_c.m_node, *p_c.m_collider, mn, mx ) ) return;
                            TAABB box;
                            box.m_min       = mn;
                            box.m_max       = mx;
                            const ImU32 col = p_c.m_collider->m_isTrigger ? IM_COL32( 255, 200, 40, 180 ) : IM_COL32( 80, 220, 80, 160 );
                            TDebugDraw::aabb( p_dl, p_state.m_viewProj, pos, size, box, col, 1.2f );
                        } );
            }

            if ( p_ctx.m_overlayVelocity )
            {
                phys->forEachBody(
                        [ & ]( const TPhysicsSystem::TBodyDebug& p_b )
                        {
                            if ( p_b.m_node == nullptr || p_b.m_body == nullptr ) return;
                            if ( !p_b.m_body->isDynamic() ) return;
                            const glm::vec3 origin( p_b.m_node->worldMatrix()[ 3 ] );
                            const glm::vec3 tip = origin + p_b.m_body->m_linearVelocity * 0.25f;
                            TDebugDraw::line( p_dl, p_state.m_viewProj, pos, size, origin, tip, IM_COL32( 255, 80, 80, 220 ), 2.0f );
                        } );
            }
        }

        void applyGizmo( TSceneEditorContext& p_ctx, const TFrameState& p_state )
        {
            TSceneNode* node = p_ctx.selectedNode();
            if ( node == nullptr ) return;

            ImGuizmo::SetOrthographic( false );
            ImGuizmo::SetDrawlist();
            ImGuizmo::SetRect( p_ctx.m_viewportPos.x, p_ctx.m_viewportPos.y, p_ctx.m_viewportSize.x, p_ctx.m_viewportSize.y );

            glm::mat4 view = p_state.m_view;
            // ImGuizmo expects OpenGL Y-up NDC; Tomos m_proj is Vulkan Y-down.
            glm::mat4 proj = p_state.m_proj;
            proj[ 1 ][ 1 ] *= -1.0f;

            glm::mat4 parent( 1.0f );
            if ( auto parentNode = node->getParent() ) parent = parentNode->worldMatrix();

            glm::mat4 world = node->worldMatrix();

            ImGuizmo::OPERATION op = ImGuizmo::TRANSLATE;
            if ( p_ctx.m_gizmoMode == TGizmoMode::Rotate )
                op = ImGuizmo::ROTATE;
            else if ( p_ctx.m_gizmoMode == TGizmoMode::Scale )
                op = ImGuizmo::SCALE;

            const ImGuizmo::MODE mode = p_ctx.m_gizmoLocal ? ImGuizmo::LOCAL : ImGuizmo::WORLD;

            if ( ImGuizmo::Manipulate( glm::value_ptr( view ), glm::value_ptr( proj ), op, mode, glm::value_ptr( world ) ) )
            {
                const glm::mat4 local = glm::inverse( parent ) * world;
                glm::vec3       t, s;
                glm::quat       r;
                t   = glm::vec3( local[ 3 ] );
                s.x = glm::length( glm::vec3( local[ 0 ] ) );
                s.y = glm::length( glm::vec3( local[ 1 ] ) );
                s.z = glm::length( glm::vec3( local[ 2 ] ) );
                const glm::mat3 rotMat( glm::vec3( local[ 0 ] ) / std::max( s.x, 1e-6f ), glm::vec3( local[ 1 ] ) / std::max( s.y, 1e-6f ),
                                        glm::vec3( local[ 2 ] ) / std::max( s.z, 1e-6f ) );
                r = glm::quat_cast( rotMat );

                node->m_transform.setLocalTRS( t, r, s );

                if ( auto* rb = node->findComponent<TRigidBodyComponent>() )
                {
                    if ( rb->isDynamic() )
                    {
                        rb->teleportTo( t );
                        rb->m_linearVelocity = glm::vec3( 0.0f );
                    }
                }
            }
        }
    }  // namespace

    void TSceneViewportPanel::draw( TSceneEditorContext& p_ctx )
    {
        ImGui::PushStyleVar( ImGuiStyleVar_WindowPadding, ImVec2( 0, 0 ) );
        ImGui::Begin( "Scene", nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse );

        ImGui::Dummy( ImVec2( 4, 0 ) );
        ImGui::SameLine();
        if ( ImGui::RadioButton( "T", p_ctx.m_gizmoMode == TGizmoMode::Translate ) ) p_ctx.m_gizmoMode = TGizmoMode::Translate;
        ImGui::SameLine();
        if ( ImGui::RadioButton( "R", p_ctx.m_gizmoMode == TGizmoMode::Rotate ) ) p_ctx.m_gizmoMode = TGizmoMode::Rotate;
        ImGui::SameLine();
        if ( ImGui::RadioButton( "S", p_ctx.m_gizmoMode == TGizmoMode::Scale ) ) p_ctx.m_gizmoMode = TGizmoMode::Scale;
        ImGui::SameLine();
        ImGui::Checkbox( "Local", &p_ctx.m_gizmoLocal );

        const ImVec2   avail  = ImGui::GetContentRegionAvail();
        const uint32_t w      = static_cast<uint32_t>( std::max( 1.0f, avail.x ) );
        const uint32_t h      = static_cast<uint32_t>( std::max( 1.0f, avail.y ) );
        p_ctx.m_desiredWidth  = w;
        p_ctx.m_desiredHeight = h;

        p_ctx.m_viewportPos     = ImGui::GetCursorScreenPos();
        p_ctx.m_viewportSize    = avail;
        p_ctx.m_viewportHovered = ImGui::IsWindowHovered( ImGuiHoveredFlags_AllowWhenBlockedByActiveItem );
        p_ctx.m_viewportFocused = ImGui::IsWindowFocused( ImGuiFocusedFlags_RootAndChildWindows );

        if ( p_ctx.m_sceneTexture != 0 && avail.x > 1.0f && avail.y > 1.0f )
        {
            ImGui::Image( p_ctx.m_sceneTexture, avail );
        }
        else
        {
            ImGui::Dummy( avail );
            ImDrawList* dl = ImGui::GetWindowDrawList();
            dl->AddRectFilled( p_ctx.m_viewportPos, ImVec2( p_ctx.m_viewportPos.x + avail.x, p_ctx.m_viewportPos.y + avail.y ), IM_COL32( 30, 30, 35, 255 ) );
            dl->AddText( ImVec2( p_ctx.m_viewportPos.x + 12, p_ctx.m_viewportPos.y + 12 ), IM_COL32( 180, 180, 180, 255 ), "Waiting for scene target…" );
        }

        auto* gpu = TApplication::get().gpu();
        if ( gpu != nullptr )
        {
            const TFrameState& state = gpu->frameState();
            ImDrawList*        dl    = ImGui::GetWindowDrawList();
            drawOverlays( p_ctx, state, dl );

            // Fly-cam disables the cursor (ImGuiConfigFlags_NoMouse). Keep
            // ImGuizmo idle in that mode — otherwise a mid-drag Tab can leave
            // IsUsing stuck and Manipulate corrupts TRS from virtual mouse deltas.
            const bool uiMouse = ( ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_NoMouse ) == 0;
            ImGuizmo::BeginFrame();
            ImGuizmo::Enable( uiMouse );
            if ( uiMouse && p_ctx.selectedNode() != nullptr ) applyGizmo( p_ctx, state );

            const bool gizmoActive = uiMouse && ( ImGuizmo::IsUsing() || ImGuizmo::IsOver() );
            if ( uiMouse && p_ctx.m_viewportHovered && ImGui::IsMouseClicked( ImGuiMouseButton_Left ) && !gizmoActive )
                pickNode( p_ctx, state, ImGui::GetIO().MousePos );

            if ( uiMouse && p_ctx.m_viewportFocused && !ImGui::GetIO().WantTextInput )
            {
                if ( ImGui::IsKeyPressed( ImGuiKey_Q ) ) p_ctx.m_gizmoMode = TGizmoMode::Translate;
                if ( ImGui::IsKeyPressed( ImGuiKey_W ) ) p_ctx.m_gizmoMode = TGizmoMode::Translate;
                if ( ImGui::IsKeyPressed( ImGuiKey_E ) ) p_ctx.m_gizmoMode = TGizmoMode::Rotate;
                if ( ImGui::IsKeyPressed( ImGuiKey_R ) ) p_ctx.m_gizmoMode = TGizmoMode::Scale;
            }
        }

        ImGui::End();
        ImGui::PopStyleVar();
    }
}  // namespace Tomos
