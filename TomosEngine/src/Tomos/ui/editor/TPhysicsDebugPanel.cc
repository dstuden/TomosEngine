#include "Tomos/ui/editor/TPhysicsDebugPanel.hh"

#include <imgui.h>

#include "Tomos/core/scene/TSceneNode.hh"
#include "Tomos/systems/physics/TPhysicsSystem.hh"

namespace Tomos
{
    void TPhysicsDebugPanel::draw( TSceneEditorContext& p_ctx )
    {
        ImGui::Begin( "Physics" );
        if ( p_ctx.m_scene == nullptr )
        {
            ImGui::TextDisabled( "No scene" );
            ImGui::End();
            return;
        }

        auto* phys = p_ctx.m_scene->ecs().maybeGetSystem<TPhysicsSystem>();
        if ( phys == nullptr )
        {
            ImGui::TextDisabled( "TPhysicsSystem not registered" );
            ImGui::End();
            return;
        }

        ImGui::DragFloat3( "Gravity", &phys->m_gravity.x, 0.05f );
        ImGui::DragFloat( "Broadphase cell", &phys->m_broadphaseCellSize, 0.05f, 0.1f, 64.0f );
        ImGui::Text( "Bodies: %zu   Colliders: %zu", phys->bodyCount(), phys->colliderCount() );
        ImGui::Text( "Fixed dt: %.4f   Accumul: %.4f   Alpha: %.3f", phys->fixedDt(), phys->accumulator(), phys->alpha() );
        ImGui::Text( "Steps last frame: %d   Contacts: %d", phys->lastStepCount(), phys->lastContactCount() );
        ImGui::Text( "Trigger events: %d   Active overlaps: %d", phys->lastTriggerEventCount(), phys->activeOverlapCount() );

        ImGui::Separator();
        ImGui::Checkbox( "Overlay colliders", &p_ctx.m_overlayColliders );
        ImGui::Checkbox( "Overlay velocity", &p_ctx.m_overlayVelocity );
        ImGui::Checkbox( "Overlay selection", &p_ctx.m_overlaySelection );

        if ( ImGui::CollapsingHeader( "Bodies", ImGuiTreeNodeFlags_DefaultOpen ) )
        {
            if ( ImGui::BeginTable( "bodies", 5, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY, ImVec2( 0, 160 ) ) )
            {
                ImGui::TableSetupColumn( "Node" );
                ImGui::TableSetupColumn( "Mode" );
                ImGui::TableSetupColumn( "Mass" );
                ImGui::TableSetupColumn( "|v|" );
                ImGui::TableSetupColumn( "Select" );
                ImGui::TableHeadersRow();
                phys->forEachBody(
                        [ & ]( const TPhysicsSystem::TBodyDebug& b )
                        {
                            if ( b.m_node == nullptr || b.m_body == nullptr ) return;
                            ImGui::TableNextRow();
                            ImGui::TableNextColumn();
                            ImGui::TextUnformatted( b.m_node->m_name.c_str() );
                            ImGui::TableNextColumn();
                            ImGui::TextUnformatted( b.m_body->isDynamic() ? "dynamic" : ( b.m_body->m_kinematic ? "kinematic" : "static" ) );
                            ImGui::TableNextColumn();
                            ImGui::Text( "%.2f", b.m_body->m_mass );
                            ImGui::TableNextColumn();
                            ImGui::Text( "%.2f", glm::length( b.m_body->m_linearVelocity ) );
                            ImGui::TableNextColumn();
                            ImGui::PushID( static_cast<int>( b.m_node->m_id ) );
                            if ( ImGui::SmallButton( "Go" ) ) p_ctx.select( b.m_node->m_id );
                            ImGui::PopID();
                        } );
                ImGui::EndTable();
            }
        }

        if ( ImGui::CollapsingHeader( "Colliders" ) )
        {
            if ( ImGui::BeginTable( "colliders", 5, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY, ImVec2( 0, 160 ) ) )
            {
                ImGui::TableSetupColumn( "Node" );
                ImGui::TableSetupColumn( "Shape" );
                ImGui::TableSetupColumn( "Flags" );
                ImGui::TableSetupColumn( "Layer/Mask" );
                ImGui::TableSetupColumn( "Select" );
                ImGui::TableHeadersRow();
                phys->forEachCollider(
                        [ & ]( const TPhysicsSystem::TColliderDebug& c )
                        {
                            if ( c.m_node == nullptr || c.m_collider == nullptr ) return;
                            ImGui::TableNextRow();
                            ImGui::TableNextColumn();
                            ImGui::TextUnformatted( c.m_node->m_name.c_str() );
                            ImGui::TableNextColumn();
                            ImGui::TextUnformatted( c.m_collider->m_shape == TColliderShape::Sphere ? "Sphere" : "Box" );
                            ImGui::TableNextColumn();
                            ImGui::Text( "%s%s", c.m_collider->m_enabled ? "" : "disabled ", c.m_collider->m_isTrigger ? "trigger" : "" );
                            ImGui::TableNextColumn();
                            ImGui::Text( "%08X / %08X", c.m_collider->m_layer, c.m_collider->m_mask );
                            ImGui::TableNextColumn();
                            ImGui::PushID( static_cast<int>( c.m_node->m_id + 100000 ) );
                            if ( ImGui::SmallButton( "Go" ) ) p_ctx.select( c.m_node->m_id );
                            ImGui::PopID();
                        } );
                ImGui::EndTable();
            }
        }

        ImGui::End();
    }
}  // namespace Tomos
