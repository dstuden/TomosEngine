#include "Tomos/ui/editor/TSceneInspectorPanel.hh"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <glm/gtc/quaternion.hpp>
#include <imgui.h>
#include <string>
#include <vector>

#include "Tomos/core/app/TApplication.hh"
#include "Tomos/systems/TComponentRegistry.hh"
#include "Tomos/systems/animation/TAnimatorComponent.hh"
#include "Tomos/systems/audio/TAudioComponent.hh"
#include "Tomos/systems/camera/TCameraComponent.hh"
#include "Tomos/systems/light/TLightComponent.hh"
#include "Tomos/systems/mesh/TMeshComponent.hh"
#include "Tomos/systems/particle/TParticleEmitterComponent.hh"
#include "Tomos/systems/physics/TColliderComponent.hh"
#include "Tomos/systems/physics/TRigidBodyComponent.hh"
#include "Tomos/systems/script/TScriptComponent.hh"
#include "Tomos/systems/script/TScriptRegistry.hh"
#include "Tomos/systems/sprite/TSpriteComponent.hh"
#include "Tomos/ui/editor/TAssetBrowserPanel.hh"
#include "Tomos/ui/editor/TSceneEditorContext.hh"

namespace Tomos
{
    namespace
    {
        void editTransform( TSceneNode& node )
        {
            TTransform& t = node.m_transform;

            float pos[ 3 ] = { t.translation().x, t.translation().y, t.translation().z };
            if ( ImGui::DragFloat3( "Position", pos, 0.05f ) )
            {
                const glm::vec3 p{ pos[ 0 ], pos[ 1 ], pos[ 2 ] };
                t.setTranslation( p );
                // Dynamic bodies own sim pose — snap + kill velocity so the
                // scrubbed position sticks under gravity between frames.
                if ( auto* rb = node.findComponent<TRigidBodyComponent>() )
                {
                    if ( rb->isDynamic() )
                    {
                        rb->teleportTo( p );
                        rb->m_linearVelocity = glm::vec3( 0.0f );
                    }
                }
            }
            // Keep re-asserting while the drag is held (physics runs before UI).
            else if ( ImGui::IsItemActive() )
            {
                if ( auto* rb = node.findComponent<TRigidBodyComponent>() )
                {
                    if ( rb->isDynamic() )
                    {
                        rb->teleportTo( t.translation() );
                        rb->m_linearVelocity = glm::vec3( 0.0f );
                    }
                }
            }

            const glm::vec3 eulerRad      = glm::eulerAngles( t.rotation() );
            float           eulerDeg[ 3 ] = { glm::degrees( eulerRad.x ), glm::degrees( eulerRad.y ), glm::degrees( eulerRad.z ) };
            if ( ImGui::DragFloat3( "Rotation (deg)", eulerDeg, 0.5f ) )
            {
                t.setRotation( glm::quat( glm::vec3( glm::radians( eulerDeg[ 0 ] ), glm::radians( eulerDeg[ 1 ] ), glm::radians( eulerDeg[ 2 ] ) ) ) );
            }

            float scale[ 3 ] = { t.scale().x, t.scale().y, t.scale().z };
            if ( ImGui::DragFloat3( "Scale", scale, 0.01f ) )
            {
                t.setScale( { scale[ 0 ], scale[ 1 ], scale[ 2 ] } );
            }
        }

        void editCamera( TCameraComponent& cam )
        {
            ImGui::Checkbox( "Active", &cam.m_active );
            int proj = cam.m_projection == TProjection::Orthographic ? 1 : 0;
            if ( ImGui::Combo( "Projection", &proj, "Perspective\0Orthographic\0" ) )
            {
                cam.m_projection = proj == 1 ? TProjection::Orthographic : TProjection::Perspective;
                cam.setDirty();
            }
            float fovDeg = glm::degrees( cam.m_fov );
            if ( ImGui::SliderFloat( "FOV (deg)", &fovDeg, 10.0f, 120.0f ) )
            {
                cam.m_fov = glm::radians( fovDeg );
                cam.setDirty();
            }
            if ( ImGui::DragFloat( "Near", &cam.m_near, 0.01f, 0.001f, 100.0f ) ) cam.setDirty();
            if ( ImGui::DragFloat( "Far", &cam.m_far, 1.0f, 1.0f, 100000.0f ) ) cam.setDirty();
            if ( ImGui::DragFloat( "Ortho half-height", &cam.m_orthoHalfHeight, 0.1f ) ) cam.setDirty();
        }

        void editLight( TLightComponent& lit )
        {
            int type = static_cast<int>( lit.m_type );
            if ( ImGui::Combo( "Type", &type, "Point\0Directional\0Spot\0" ) ) lit.m_type = static_cast<TLightType>( type );
            ImGui::ColorEdit3( "Color", &lit.m_color.x );
            ImGui::DragFloat( "Intensity", &lit.m_intensity, 0.1f, 0.0f, 1000.0f );
            ImGui::DragFloat( "Max range", &lit.m_maxRange, 0.1f, 0.1f, 1000.0f );
            float innerDeg = glm::degrees( lit.m_innerCone );
            float outerDeg = glm::degrees( lit.m_outerCone );
            if ( ImGui::SliderFloat( "Inner cone (deg)", &innerDeg, 0.0f, 89.0f ) ) lit.m_innerCone = glm::radians( innerDeg );
            if ( ImGui::SliderFloat( "Outer cone (deg)", &outerDeg, 1.0f, 90.0f ) ) lit.m_outerCone = glm::radians( outerDeg );
            ImGui::Checkbox( "Cast shadow", &lit.m_castShadow );
        }

        void editMesh( TMeshComponent& mesh, bool skinned )
        {
            ImGui::Text( "Mesh ptr: %s", mesh.m_mesh ? "bound" : "null" );
            ImGui::Text( "Material: %s", mesh.m_material ? "bound" : "null" );
            ImGui::Checkbox( "Cast shadow", &mesh.m_castShadow );

            auto& assets = TApplication::get().assetSystem();
            auto  names  = assets.assetNames();
            if ( names.empty() )
            {
                ImGui::TextDisabled( "No registered assets" );
                return;
            }

            TMeshAssetRef ref{};
            const bool    resolved = assets.resolveMesh( mesh.m_mesh, mesh.m_material, ref );
            int           assetIdx = 0;
            for ( size_t i = 0; i < names.size(); ++i )
            {
                if ( resolved && names[ i ] == ref.m_assetName ) assetIdx = static_cast<int>( i );
            }

            std::vector<const char*> namePtrs;
            namePtrs.reserve( names.size() );
            for ( const auto& n : names ) namePtrs.push_back( n.c_str() );

            bool changed = ImGui::Combo( "Asset", &assetIdx, namePtrs.data(), static_cast<int>( namePtrs.size() ) );
            int  meshIdx = resolved ? static_cast<int>( ref.m_meshIdx ) : 0;
            int  matIdx  = resolved ? static_cast<int>( ref.m_materialIdx ) : 0;

            TGpuAsset* asset = assets.maybeGetAsset( names[ static_cast<size_t>( assetIdx ) ] );
            if ( asset != nullptr )
            {
                const int meshCount = static_cast<int>( asset->m_meshes.size() );
                const int matCount  = static_cast<int>( asset->m_materials.size() );
                if ( meshCount > 0 )
                {
                    meshIdx = std::clamp( meshIdx, 0, meshCount - 1 );
                    changed = ImGui::SliderInt( "Mesh index", &meshIdx, 0, meshCount - 1 ) || changed;
                }
                if ( matCount > 0 )
                {
                    matIdx  = std::clamp( matIdx, 0, matCount - 1 );
                    changed = ImGui::SliderInt( "Material index", &matIdx, 0, matCount - 1 ) || changed;
                }
                if ( changed )
                {
                    mesh.m_mesh     = meshCount > 0 ? asset->mesh( static_cast<uint32_t>( meshIdx ) ) : nullptr;
                    mesh.m_material = matCount > 0 ? asset->material( static_cast<uint32_t>( matIdx ) ) : nullptr;
                }
            }

            if ( skinned )
            {
                auto* sk = dynamic_cast<TSkinnedMeshComponent*>( &mesh );
                if ( sk != nullptr ) ImGui::Text( "Joints: %zu", sk->m_joints.size() );
            }
        }

        void editAnimator( TAnimatorComponent& anim )
        {
            ImGui::Checkbox( "Playing", &anim.m_playing );
            ImGui::Checkbox( "Loop", &anim.m_looping );
            ImGui::SliderFloat( "Speed", &anim.m_speed, 0.0f, 3.0f );
            ImGui::SliderFloat( "Default fade", &anim.m_defaultFadeDuration, 0.0f, 2.0f );
            const float duration = anim.m_clip != nullptr ? anim.m_clip->m_duration : 0.0f;
            float       t        = anim.m_time;
            if ( ImGui::SliderFloat( "Time", &t, 0.0f, duration > 0.0f ? duration : 1.0f ) ) anim.seek( t );

            if ( anim.isCrossfading() )
                ImGui::Text( "Crossfade: %.0f%%", anim.m_blendWeight * 100.0f );
            if ( anim.m_stateMachine.m_enabled )
                ImGui::Text( "State: %s", anim.m_stateMachine.m_current.c_str() );

            auto& assets = TApplication::get().assetSystem();
            auto  names  = assets.assetNames();
            if ( names.empty() ) return;

            std::string assetName, clipName;
            ( void ) assets.resolveClip( anim.m_clip, assetName, clipName );

            int assetIdx = 0;
            for ( size_t i = 0; i < names.size(); ++i )
                if ( names[ i ] == assetName ) assetIdx = static_cast<int>( i );

            std::vector<const char*> namePtrs;
            for ( const auto& n : names ) namePtrs.push_back( n.c_str() );
            if ( ImGui::Combo( "Anim asset", &assetIdx, namePtrs.data(), static_cast<int>( namePtrs.size() ) ) )
            {
                TGpuAsset* asset = assets.maybeGetAsset( names[ static_cast<size_t>( assetIdx ) ] );
                if ( asset != nullptr && !asset->m_clips.empty() ) anim.play( asset->m_clips.front().get() );
            }

            TGpuAsset* asset = assets.maybeGetAsset( names[ static_cast<size_t>( assetIdx ) ] );
            if ( asset != nullptr && !asset->m_clips.empty() )
            {
                int                      clipIdx = 0;
                std::vector<const char*> clipPtrs;
                for ( size_t i = 0; i < asset->m_clips.size(); ++i )
                {
                    clipPtrs.push_back( asset->m_clips[ i ]->m_name.c_str() );
                    if ( anim.m_clip == asset->m_clips[ i ].get() ) clipIdx = static_cast<int>( i );
                }
                if ( ImGui::Combo( "Clip", &clipIdx, clipPtrs.data(), static_cast<int>( clipPtrs.size() ) ) )
                    anim.crossfadeTo( asset->m_clips[ static_cast<size_t>( clipIdx ) ].get() );
            }
        }

        void editSprite( TSpriteComponent& spr, TSceneEditorContext& ctx )
        {
            ImGui::DragFloat2( "Size", &spr.m_size.x, 0.01f );
            ImGui::ColorEdit4( "Color", &spr.m_color.x );
            int mode = static_cast<int>( spr.m_mode );
            if ( ImGui::Combo( "Billboard", &mode, "Spherical\0Cylindrical\0Fixed\0" ) ) spr.m_mode = static_cast<TBillboardMode>( mode );
            float rotDeg = glm::degrees( spr.m_rotation );
            if ( ImGui::DragFloat( "Rotation (deg)", &rotDeg, 0.5f ) ) spr.m_rotation = glm::radians( rotDeg );
            ImGui::DragFloat2( "UV min", &spr.m_uvMin.x, 0.01f );
            ImGui::DragFloat2( "UV max", &spr.m_uvMax.x, 0.01f );
            ImGui::Checkbox( "Visible", &spr.m_visible );

            std::string texPath;
            if ( ctx.m_bag != nullptr )
            {
                if ( const std::string* p = ctx.m_bag->findImagePath( spr.m_texture ) ) texPath = *p;
            }
            char pathBuf[ 512 ];
            std::snprintf( pathBuf, sizeof( pathBuf ), "%s", texPath.c_str() );
            if ( ImGui::InputText( "Texture path", pathBuf, sizeof( pathBuf ), ImGuiInputTextFlags_EnterReturnsTrue ) )
            {
                if ( ctx.m_bag != nullptr && TApplication::get().gpu() != nullptr ) spr.m_texture = ctx.m_bag->loadImage( *TApplication::get().gpu(), pathBuf );
            }
            ImGui::Text( "Texture: %s", spr.m_texture ? "bound" : "nullptr" );
        }

        void editParticle( TParticleEmitterComponent& p, TSceneEditorContext& ctx )
        {
            ImGui::Checkbox( "Emitting", &p.m_emitting );
            ImGui::SliderFloat( "Rate", &p.m_rate, 0.0f, 400.0f );
            ImGui::SliderFloat( "Gravity", &p.m_gravity, -20.0f, 5.0f );
            ImGui::SliderFloat( "Life min", &p.m_lifetimeMin, 0.05f, 3.0f );
            ImGui::SliderFloat( "Life max", &p.m_lifetimeMax, 0.05f, 4.0f );
            ImGui::DragFloat3( "Vel min", &p.m_velocityMin.x, 0.05f );
            ImGui::DragFloat3( "Vel max", &p.m_velocityMax.x, 0.05f );
            ImGui::DragFloat2( "Size start", &p.m_sizeStart.x, 0.01f );
            ImGui::DragFloat2( "Size end", &p.m_sizeEnd.x, 0.01f );
            ImGui::ColorEdit4( "Color start", &p.m_colorStart.x );
            ImGui::ColorEdit4( "Color end", &p.m_colorEnd.x );
            ImGui::DragFloat2( "UV min", &p.m_uvMin.x, 0.01f );
            ImGui::DragFloat2( "UV max", &p.m_uvMax.x, 0.01f );
            int seed = static_cast<int>( p.m_seed );
            if ( ImGui::DragInt( "Seed", &seed, 1, 1, 1000000 ) ) p.m_seed = static_cast<uint32_t>( std::max( 1, seed ) );
            if ( ImGui::Button( "Burst 200" ) ) p.burst( 200 );

            std::string texPath;
            if ( ctx.m_bag != nullptr )
            {
                if ( const std::string* path = ctx.m_bag->findImagePath( p.m_texture ) ) texPath = *path;
            }
            char pathBuf[ 512 ];
            std::snprintf( pathBuf, sizeof( pathBuf ), "%s", texPath.c_str() );
            if ( ImGui::InputText( "Texture path", pathBuf, sizeof( pathBuf ), ImGuiInputTextFlags_EnterReturnsTrue ) )
            {
                if ( ctx.m_bag != nullptr && TApplication::get().gpu() != nullptr ) p.m_texture = ctx.m_bag->loadImage( *TApplication::get().gpu(), pathBuf );
            }
            ImGui::Text( "Texture: %s", p.m_texture ? "bound" : "nullptr" );
        }

        void editAudio( TAudioComponent& a, TSceneEditorContext& ctx )
        {
            ImGui::Checkbox( "Spatial", &a.m_spatial );
            ImGui::Checkbox( "Looping", &a.m_looping );
            ImGui::Checkbox( "Play on start", &a.m_playOnStart );
            ImGui::SliderFloat( "Volume", &a.m_volume, 0.0f, 1.0f );
            ImGui::SliderFloat( "Pitch", &a.m_pitch, 0.25f, 2.0f );
            ImGui::SliderFloat( "Min distance", &a.m_minDistance, 0.1f, 10.0f );
            ImGui::SliderFloat( "Max distance", &a.m_maxDistance, 1.0f, 80.0f );

            std::string clipPath;
            if ( ctx.m_bag != nullptr )
            {
                if ( const std::string* p = ctx.m_bag->findClipPath( a.m_clip ) ) clipPath = *p;
            }
            char pathBuf[ 512 ];
            std::snprintf( pathBuf, sizeof( pathBuf ), "%s", clipPath.c_str() );
            if ( ImGui::InputText( "Clip path", pathBuf, sizeof( pathBuf ), ImGuiInputTextFlags_EnterReturnsTrue ) )
            {
                if ( ctx.m_bag != nullptr ) a.m_clip = ctx.m_bag->getOrCreateClip( pathBuf );
            }
            ImGui::Text( "Clip: %s", a.m_clip ? a.m_clip->m_name.c_str() : "(none)" );
            ImGui::Text( "Playing: %s", a.m_playing ? "yes" : "no" );
            if ( ImGui::Button( "Play" ) ) a.play( true );
            ImGui::SameLine();
            if ( ImGui::Button( "Stop" ) ) a.stop();
        }

        void editRigidBody( TRigidBodyComponent& b )
        {
            float mass = b.m_mass;
            if ( ImGui::DragFloat( "Mass", &mass, 0.05f, 0.0f, 1000.0f ) ) b.setMass( mass );
            ImGui::Checkbox( "Use gravity", &b.m_useGravity );
            ImGui::Checkbox( "Kinematic", &b.m_kinematic );
            ImGui::DragFloat( "Gravity scale", &b.m_gravityScale, 0.05f, 0.0f, 10.0f );
            ImGui::DragFloat( "Linear damping", &b.m_linearDamping, 0.01f, 0.0f, 1.0f );
            ImGui::SliderFloat( "Restitution", &b.m_restitution, 0.0f, 1.0f );
            ImGui::DragFloat3( "Velocity", &b.m_linearVelocity.x, 0.05f );
            if ( ImGui::Button( "Zero velocity" ) ) b.m_linearVelocity = glm::vec3( 0.0f );
        }

        void editCollider( TColliderComponent& col )
        {
            int shape = static_cast<int>( col.m_shape );
            if ( ImGui::Combo( "Shape", &shape, "Sphere\0Box\0" ) ) col.m_shape = static_cast<TColliderShape>( shape );
            ImGui::Checkbox( "Enabled", &col.m_enabled );
            ImGui::Checkbox( "Trigger", &col.m_isTrigger );
            if ( col.m_shape == TColliderShape::Sphere )
            {
                ImGui::DragFloat( "Radius (local)", &col.m_radius, 0.01f, 0.0f, 100.0f );
                ImGui::TextDisabled( "× node scale; 0.5 hugs unit sphere" );
            }
            else
            {
                ImGui::DragFloat3( "Half extents (local)", &col.m_halfExtents.x, 0.01f, 0.0f, 100.0f );
                ImGui::TextDisabled( "× node scale; 0.5 hugs unit box. 0 = no volume" );
            }

            ImGui::Separator();
            ImGui::TextUnformatted( "Collision filter" );
            ImGui::TextDisabled( "Pair if (A.layer & B.mask) && (B.layer & A.mask)" );
            auto editBits = []( const char* label, uint32_t& bits )
            {
                ImGui::TextUnformatted( label );
                ImGui::PushID( label );
                for ( int i = 0; i < 8; ++i )
                {
                    if ( i > 0 ) ImGui::SameLine();
                    bool on = ( bits & ( 1u << i ) ) != 0;
                    char buf[ 4 ];
                    std::snprintf( buf, sizeof( buf ), "%d", i );
                    if ( ImGui::Checkbox( buf, &on ) )
                    {
                        if ( on )
                            bits |= ( 1u << i );
                        else
                            bits &= ~( 1u << i );
                    }
                }
                ImGui::InputScalar( "hex", ImGuiDataType_U32, &bits, nullptr, nullptr, "%08X", ImGuiInputTextFlags_CharsHexadecimal );
                ImGui::PopID();
            };
            editBits( "Layer", col.m_layer );
            editBits( "Mask", col.m_mask );
        }
    }  // namespace

    void TSceneInspectorPanel::draw( TSceneEditorContext& p_ctx )
    {
        ImGui::Begin( "Inspector" );
        TSceneNode* node = p_ctx.selectedNode();
        if ( node == nullptr )
        {
            ImGui::TextDisabled( "Select a node in the Hierarchy" );
            ImGui::End();
            return;
        }

        ImGui::Text( "Id: %llu", static_cast<unsigned long long>( node->m_id ) );

        char nameBuf[ 256 ];
        std::snprintf( nameBuf, sizeof( nameBuf ), "%s", node->m_name.c_str() );
        if ( ImGui::InputText( "Name", nameBuf, sizeof( nameBuf ) ) ) node->m_name = nameBuf;
        ImGui::Checkbox( "Dynamic", &node->m_dynamic );

        if ( ImGui::CollapsingHeader( "Transform", ImGuiTreeNodeFlags_DefaultOpen ) )
        {
            editTransform( *node );
            const glm::mat4 world = node->worldMatrix();
            const glm::vec3 wpos( world[ 3 ] );
            ImGui::TextDisabled( "World pos: %.2f  %.2f  %.2f", wpos.x, wpos.y, wpos.z );
        }

        ImGui::Separator();
        ImGui::Text( "Components (%zu)", node->getComponents().size() );

        std::vector<TComponent*> comps;
        for ( const auto& c : node->getComponents() ) comps.push_back( c.get() );

        for ( size_t i = 0; i < comps.size(); ++i )
        {
            TComponent* c        = comps[ i ];
            std::string typeName = TComponentRegistry::get().typeOf( *c );
            if ( typeName.empty() )
            {
                if ( dynamic_cast<TScriptComponent*>( c ) != nullptr )
                    typeName = "script";
                else
                    typeName = "unknown";
            }

            ImGui::PushID( static_cast<int>( i ) );
            const bool open = ImGui::CollapsingHeader( typeName.c_str(), ImGuiTreeNodeFlags_DefaultOpen );
            if ( open )
            {
                if ( auto* cam = dynamic_cast<TCameraComponent*>( c ) )
                    editCamera( *cam );
                else if ( auto* lit = dynamic_cast<TLightComponent*>( c ) )
                    editLight( *lit );
                else if ( auto* sk = dynamic_cast<TSkinnedMeshComponent*>( c ) )
                    editMesh( *sk, true );
                else if ( auto* mesh = dynamic_cast<TMeshComponent*>( c ) )
                    editMesh( *mesh, false );
                else if ( auto* anim = dynamic_cast<TAnimatorComponent*>( c ) )
                    editAnimator( *anim );
                else if ( auto* spr = dynamic_cast<TSpriteComponent*>( c ) )
                    editSprite( *spr, p_ctx );
                else if ( auto* part = dynamic_cast<TParticleEmitterComponent*>( c ) )
                    editParticle( *part, p_ctx );
                else if ( auto* aud = dynamic_cast<TAudioComponent*>( c ) )
                    editAudio( *aud, p_ctx );
                else if ( auto* rb = dynamic_cast<TRigidBodyComponent*>( c ) )
                    editRigidBody( *rb );
                else if ( auto* col = dynamic_cast<TColliderComponent*>( c ) )
                    editCollider( *col );
                else if ( auto* sc = dynamic_cast<TScriptComponent*>( c ) )
                {
                    const std::string name = sc->typeName();
                    if ( name.empty() )
                        ImGui::TextDisabled( "Unregistered script (not serialized)." );
                    else
                        ImGui::Text( "Type: %s", name.c_str() );
                    ImGui::TextDisabled( "Register with TScriptRegistry for save/load." );
                }

                if ( ImGui::SmallButton( "Remove" ) ) node->removeComponent( c );
            }
            ImGui::PopID();
        }

        ImGui::Separator();
        if ( ImGui::BeginCombo( "Add Component", "Select…" ) )
        {
            for ( const auto& info : TComponentRegistry::get().types() )
            {
                if ( ImGui::Selectable( info.m_label.c_str() ) )
                {
                    if ( auto comp = TComponentRegistry::get().createDefault( info.m_type ) ) node->addComponent( std::move( comp ) );
                }
            }
            ImGui::EndCombo();
        }

        if ( ImGui::BeginCombo( "Add Script", "Select…" ) )
        {
            const auto& names = TScriptRegistry::get().names();
            if ( names.empty() )
                ImGui::TextDisabled( "No scripts registered" );
            else
            {
                for ( const auto& name : names )
                {
                    if ( ImGui::Selectable( name.c_str() ) )
                    {
                        if ( auto comp = TScriptComponent::makeFromType( name ) ) node->addComponent( std::move( comp ) );
                    }
                }
            }
            ImGui::EndCombo();
        }

        ImGui::Separator();
        ImGui::TextDisabled( "Drop mesh / audio / texture here" );
        if ( ImGui::BeginDragDropTarget() )
        {
            if ( const ImGuiPayload* payload = ImGui::AcceptDragDropPayload( TAssetBrowserPanel::k_payloadMesh ) )
                TAssetBrowserPanel::applyDrop( p_ctx, *node, *payload );
            if ( const ImGuiPayload* payload = ImGui::AcceptDragDropPayload( TAssetBrowserPanel::k_payloadGpuAsset ) )
                TAssetBrowserPanel::applyDrop( p_ctx, *node, *payload );
            if ( const ImGuiPayload* payload = ImGui::AcceptDragDropPayload( TAssetBrowserPanel::k_payloadAudio ) )
                TAssetBrowserPanel::applyDrop( p_ctx, *node, *payload );
            if ( const ImGuiPayload* payload = ImGui::AcceptDragDropPayload( TAssetBrowserPanel::k_payloadTexture ) )
                TAssetBrowserPanel::applyDrop( p_ctx, *node, *payload );
            ImGui::EndDragDropTarget();
        }

        ImGui::End();
    }
}  // namespace Tomos
