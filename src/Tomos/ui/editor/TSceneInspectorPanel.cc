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
#include "Tomos/util/image/TAnimatedTexture.hh"
#include "Tomos/util/reflect/TReflectEnum.hh"
#include "Tomos/util/reflect/TReflectImGui.hh"

namespace Tomos
{
    namespace
    {
        void editAnimPlayback( TSceneEditorContext& p_ctx, const TVkImage* p_tex, TBagAnimatedTextureRef& p_ref )
        {
            if ( p_ctx.m_bag == nullptr || p_tex == nullptr ) return;
            TAnimatedTexture* anim = p_ctx.m_bag->findAnimatedTexture( p_tex );
            if ( anim == nullptr )
            {
                ImGui::TextDisabled( "Static texture" );
                return;
            }
            ImGui::Separator();
            ImGui::TextUnformatted( "Animated texture" );
            if ( ImGui::Checkbox( "Playing##anim", &anim->m_playing ) ) p_ref.m_playing = anim->m_playing;
            if ( ImGui::Checkbox( "Loop##anim", &anim->m_looping ) ) p_ref.m_looping = anim->m_looping;
            if ( ImGui::SliderFloat( "Speed##anim", &anim->m_speed, 0.0f, 4.0f ) ) p_ref.m_speed = anim->m_speed;
            p_ref.m_path = anim->path();
        }

        void editTexturePath( TSceneEditorContext& p_ctx, const TVkImage*& p_tex, TBagTextureRef* p_pathRef, TBagAnimatedTextureRef& p_animRef,
                              const char* p_label = "Texture path" )
        {
            std::string texPath = p_animRef.m_path;
            if ( texPath.empty() && p_ctx.m_bag != nullptr )
            {
                if ( const std::string* p = p_ctx.m_bag->findImagePath( p_tex ) ) texPath = *p;
            }
            char pathBuf[ 512 ];
            std::snprintf( pathBuf, sizeof( pathBuf ), "%s", texPath.c_str() );
            if ( ImGui::InputText( p_label, pathBuf, sizeof( pathBuf ), ImGuiInputTextFlags_EnterReturnsTrue ) )
            {
                if ( p_ctx.m_bag != nullptr && TApplication::get().gpu() != nullptr )
                {
                    p_animRef.m_path = pathBuf;
                    p_tex            = p_ctx.m_bag->resolveTexture( *TApplication::get().gpu(), pathBuf, p_animRef );
                    if ( p_pathRef != nullptr ) p_pathRef->m_path = pathBuf;
                    if ( auto* anim = p_ctx.m_bag->findAnimatedTexture( p_tex ) ) anim->applyOpts( p_animRef );
                }
            }
            ImGui::Text( "Texture: %s", p_tex ? "bound" : "nullptr" );
            editAnimPlayback( p_ctx, p_tex, p_animRef );
        }

        void editTransform( TSceneNode& p_node )
        {
            TTransform& t = p_node.m_transform;

            float pos[ 3 ] = { t.translation().x, t.translation().y, t.translation().z };
            if ( ImGui::DragFloat3( "Position", pos, 0.05f ) )
            {
                const glm::vec3 p{ pos[ 0 ], pos[ 1 ], pos[ 2 ] };
                t.setTranslation( p );
                // Dynamic bodies own sim pose — snap + kill velocity so the
                // scrubbed position sticks under gravity between frames.
                if ( auto* rb = p_node.findComponent<TRigidBodyComponent>() )
                {
                    if ( rb->isDynamic() )
                    {
                        // Physics expects world translation (nested parents).
                        rb->teleportTo( glm::vec3( p_node.worldMatrix()[ 3 ] ) );
                        rb->m_linearVelocity = glm::vec3( 0.0f );
                    }
                }
            }
            // Keep re-asserting while the drag is held (physics runs before UI).
            else if ( ImGui::IsItemActive() )
            {
                if ( auto* rb = p_node.findComponent<TRigidBodyComponent>() )
                {
                    if ( rb->isDynamic() )
                    {
                        rb->teleportTo( glm::vec3( p_node.worldMatrix()[ 3 ] ) );
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

        void editCamera( TCameraComponent& p_cam )
        {
            if ( Reflect::editFields( p_cam ) ) p_cam.setDirty();
        }

        void editLight( TLightComponent& p_lit ) { Reflect::editFields( p_lit ); }

        void editMesh( TMeshComponent& p_mesh, bool p_skinned, TSceneEditorContext& p_ctx )
        {
            ImGui::Text( "Mesh ptr: %s", p_mesh.m_mesh ? "bound" : "null" );
            ImGui::Text( "Material: %s", p_mesh.m_material ? "bound" : "null" );
            ImGui::Checkbox( "Cast shadow", &p_mesh.m_castShadow );

            auto& assets = TApplication::get().assetSystem();
            auto  names  = assets.assetNames();
            if ( names.empty() )
            {
                ImGui::TextDisabled( "No registered assets" );
            }
            else
            {
                TMeshAssetRef ref{};
                const bool    resolved = assets.resolveMesh( p_mesh.m_mesh, p_mesh.m_material, ref );
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
                        p_mesh.m_mesh     = meshCount > 0 ? asset->mesh( static_cast<uint32_t>( meshIdx ) ) : nullptr;
                        p_mesh.m_material = matCount > 0 ? asset->material( static_cast<uint32_t>( matIdx ) ) : nullptr;
                    }
                }
            }

            if ( p_skinned )
            {
                auto* sk = dynamic_cast<TSkinnedMeshComponent*>( &p_mesh );
                if ( sk != nullptr ) ImGui::Text( "Joints: %zu", sk->m_joints.size() );
            }

            ImGui::Separator();
            ImGui::TextUnformatted( "Texture overrides" );
            {
                char pathBuf[ 512 ];
                std::snprintf( pathBuf, sizeof( pathBuf ), "%s", p_mesh.m_baseTextureOverride.m_path.c_str() );
                if ( ImGui::InputText( "Base override", pathBuf, sizeof( pathBuf ), ImGuiInputTextFlags_EnterReturnsTrue ) )
                {
                    p_mesh.m_baseTextureOverride.m_path = pathBuf;
                    if ( p_ctx.m_bag != nullptr && TApplication::get().gpu() != nullptr )
                        p_mesh.rebindOverrides( *p_ctx.m_bag, *TApplication::get().gpu() );
                }
                editAnimPlayback( p_ctx, p_mesh.m_baseOverrideImage, p_mesh.m_baseTextureOverride );
            }
            {
                char pathBuf[ 512 ];
                std::snprintf( pathBuf, sizeof( pathBuf ), "%s", p_mesh.m_emissionTextureOverride.m_path.c_str() );
                if ( ImGui::InputText( "Emission override", pathBuf, sizeof( pathBuf ), ImGuiInputTextFlags_EnterReturnsTrue ) )
                {
                    p_mesh.m_emissionTextureOverride.m_path = pathBuf;
                    if ( p_ctx.m_bag != nullptr && TApplication::get().gpu() != nullptr )
                        p_mesh.rebindOverrides( *p_ctx.m_bag, *TApplication::get().gpu() );
                }
                editAnimPlayback( p_ctx, p_mesh.m_emissionOverrideImage, p_mesh.m_emissionTextureOverride );
            }
        }

        void editAnimator( TAnimatorComponent& p_anim )
        {
            ImGui::Checkbox( "Playing", &p_anim.m_playing );
            ImGui::Checkbox( "Loop", &p_anim.m_looping );
            ImGui::SliderFloat( "Speed", &p_anim.m_speed, 0.0f, 3.0f );
            ImGui::SliderFloat( "Default fade", &p_anim.m_defaultFadeDuration, 0.0f, 2.0f );
            const float duration = p_anim.m_clip != nullptr ? p_anim.m_clip->m_duration : 0.0f;
            float       t        = p_anim.m_time;
            if ( ImGui::SliderFloat( "Time", &t, 0.0f, duration > 0.0f ? duration : 1.0f ) ) p_anim.seek( t );

            if ( p_anim.isCrossfading() ) ImGui::Text( "Crossfade: %.0f%%", p_anim.m_blendWeight * 100.0f );
            if ( p_anim.m_stateMachine.m_enabled ) ImGui::Text( "State: %s", p_anim.m_stateMachine.m_current.c_str() );

            auto& assets = TApplication::get().assetSystem();
            auto  names  = assets.assetNames();
            if ( names.empty() ) return;

            std::string assetName, clipName;
            ( void ) assets.resolveClip( p_anim.m_clip, assetName, clipName );

            int assetIdx = 0;
            for ( size_t i = 0; i < names.size(); ++i )
                if ( names[ i ] == assetName ) assetIdx = static_cast<int>( i );

            std::vector<const char*> namePtrs;
            namePtrs.reserve( names.size() );
            for ( const auto& n : names ) namePtrs.push_back( n.c_str() );
            if ( ImGui::Combo( "Anim asset", &assetIdx, namePtrs.data(), static_cast<int>( namePtrs.size() ) ) )
            {
                TGpuAsset* asset = assets.maybeGetAsset( names[ static_cast<size_t>( assetIdx ) ] );
                if ( asset != nullptr && !asset->m_clips.empty() ) p_anim.play( asset->m_clips.front().get() );
            }

            TGpuAsset* asset = assets.maybeGetAsset( names[ static_cast<size_t>( assetIdx ) ] );
            if ( asset != nullptr && !asset->m_clips.empty() )
            {
                int                      clipIdx = 0;
                std::vector<const char*> clipPtrs;
                for ( size_t i = 0; i < asset->m_clips.size(); ++i )
                {
                    clipPtrs.push_back( asset->m_clips[ i ]->m_name.c_str() );
                    if ( p_anim.m_clip == asset->m_clips[ i ].get() ) clipIdx = static_cast<int>( i );
                }
                if ( ImGui::Combo( "Clip", &clipIdx, clipPtrs.data(), static_cast<int>( clipPtrs.size() ) ) )
                    p_anim.crossfadeTo( asset->m_clips[ static_cast<size_t>( clipIdx ) ].get() );
            }
        }

        void editSprite( TSpriteComponent& p_spr, TSceneEditorContext& p_ctx )
        {
            ImGui::DragFloat2( "Size", &p_spr.m_size.x, 0.01f );
            ImGui::ColorEdit4( "Color", &p_spr.m_color.x );
            int mode = Reflect::enumIndex( p_spr.m_mode );
            if ( ImGui::Combo( "Billboard", &mode, Reflect::enumImGuiItems<TBillboardMode>() ) )
                p_spr.m_mode = Reflect::enumFromIndex<TBillboardMode>( mode );
            int alphaMode = Reflect::enumIndex( p_spr.m_alphaMode );
            if ( ImGui::Combo( "Alpha", &alphaMode, "Cutout (writes depth)\0Blend (sorted, no depth)\0" ) )
                p_spr.m_alphaMode = Reflect::enumFromIndex<TSpriteAlphaMode>( alphaMode );
            if ( p_spr.m_alphaMode == TSpriteAlphaMode::Cutout ) ImGui::SliderFloat( "Alpha cutoff", &p_spr.m_alphaCutoff, 0.01f, 1.0f );
            float rotDeg = glm::degrees( p_spr.m_rotation );
            if ( ImGui::DragFloat( "Rotation (deg)", &rotDeg, 0.5f ) ) p_spr.m_rotation = glm::radians( rotDeg );
            ImGui::DragFloat2( "UV min", &p_spr.m_uvMin.x, 0.01f );
            ImGui::DragFloat2( "UV max", &p_spr.m_uvMax.x, 0.01f );
            ImGui::Checkbox( "Visible", &p_spr.m_visible );
            editTexturePath( p_ctx, p_spr.m_texture, &p_spr.m_textureRef, p_spr.m_animRef );
        }

        void editParticle( TParticleEmitterComponent& p_p, TSceneEditorContext& p_ctx )
        {
            Reflect::editFields( p_p );
            if ( ImGui::Button( "Burst 200" ) ) p_p.burst( 200 );
            editTexturePath( p_ctx, p_p.m_texture, nullptr, p_p.m_animRef );
        }

        void editAudio( TAudioComponent& p_a, TSceneEditorContext& p_ctx )
        {
            ImGui::Checkbox( "Spatial", &p_a.m_spatial );
            ImGui::Checkbox( "Looping", &p_a.m_looping );
            ImGui::Checkbox( "Play on start", &p_a.m_playOnStart );
            ImGui::SliderFloat( "Volume", &p_a.m_volume, 0.0f, 1.0f );
            ImGui::SliderFloat( "Pitch", &p_a.m_pitch, 0.25f, 2.0f );
            ImGui::SliderFloat( "Min distance", &p_a.m_minDistance, 0.1f, 10.0f );
            ImGui::SliderFloat( "Max distance", &p_a.m_maxDistance, 1.0f, 80.0f );

            std::string clipPath;
            if ( p_ctx.m_bag != nullptr )
            {
                if ( const std::string* p = p_ctx.m_bag->findClipPath( p_a.m_clip ) ) clipPath = *p;
            }
            char pathBuf[ 512 ];
            std::snprintf( pathBuf, sizeof( pathBuf ), "%s", clipPath.c_str() );
            if ( ImGui::InputText( "Clip path", pathBuf, sizeof( pathBuf ), ImGuiInputTextFlags_EnterReturnsTrue ) )
            {
                if ( p_ctx.m_bag != nullptr ) p_a.m_clip = p_ctx.m_bag->getOrCreateClip( pathBuf );
            }
            ImGui::Text( "Clip: %s", p_a.m_clip ? p_a.m_clip->m_name.c_str() : "(none)" );
            ImGui::Text( "Playing: %s", p_a.m_playing ? "yes" : "no" );
            if ( ImGui::Button( "Play" ) ) p_a.play( true );
            ImGui::SameLine();
            if ( ImGui::Button( "Stop" ) ) p_a.stop();
        }

        void editRigidBody( TRigidBodyComponent& p_b )
        {
            float mass = p_b.m_mass;
            if ( ImGui::DragFloat( "Mass", &mass, 0.05f, 0.0f, 1000.0f ) ) p_b.setMass( mass );
            Reflect::editFields( p_b );
            if ( ImGui::Button( "Zero velocity" ) ) p_b.m_linearVelocity = glm::vec3( 0.0f );
        }

        void editCollider( TColliderComponent& p_col )
        {
            int shape = Reflect::enumIndex( p_col.m_shape );
            if ( ImGui::Combo( "Shape", &shape, Reflect::enumImGuiItems<TColliderShape>() ) )
                p_col.m_shape = Reflect::enumFromIndex<TColliderShape>( shape );
            ImGui::Checkbox( "Enabled", &p_col.m_enabled );
            ImGui::Checkbox( "Trigger", &p_col.m_isTrigger );
            if ( p_col.m_shape == TColliderShape::Sphere )
            {
                ImGui::DragFloat( "Radius (local)", &p_col.m_radius, 0.01f, 0.0f, 100.0f );
                ImGui::TextDisabled( "× node scale; 0.5 hugs unit sphere" );
            }
            else
            {
                ImGui::DragFloat3( "Half extents (local)", &p_col.m_halfExtents.x, 0.01f, 0.0f, 100.0f );
                ImGui::TextDisabled( "× node scale; 0.5 hugs unit box. 0 = no volume" );
            }

            ImGui::Separator();
            ImGui::TextUnformatted( "Collision filter" );
            ImGui::TextDisabled( "Pair if (A.layer & B.mask) && (B.layer & A.mask)" );
            auto editBits = []( const char* p_label, uint32_t& p_bits )
            {
                ImGui::TextUnformatted( p_label );
                ImGui::PushID( p_label );
                for ( int i = 0; i < 8; ++i )
                {
                    if ( i > 0 ) ImGui::SameLine();
                    bool on = ( p_bits & ( 1u << i ) ) != 0;
                    char buf[ 4 ];
                    std::snprintf( buf, sizeof( buf ), "%d", i );
                    if ( ImGui::Checkbox( buf, &on ) )
                    {
                        if ( on )
                            p_bits |= ( 1u << i );
                        else
                            p_bits &= ~( 1u << i );
                    }
                }
                ImGui::InputScalar( "hex", ImGuiDataType_U32, &p_bits, nullptr, nullptr, "%08X", ImGuiInputTextFlags_CharsHexadecimal );
                ImGui::PopID();
            };
            editBits( "Layer", p_col.m_layer );
            editBits( "Mask", p_col.m_mask );
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

        const std::vector<TComponent*>& comps = node->getComponents();

        for ( size_t i = 0; i < comps.size(); ++i )
        {
            TComponent* c = comps[ i ];
            if ( c == nullptr ) continue;
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
                    editMesh( *sk, true, p_ctx );
                else if ( auto* mesh = dynamic_cast<TMeshComponent*>( c ) )
                    editMesh( *mesh, false, p_ctx );
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
            if ( const ImGuiPayload* payload = ImGui::AcceptDragDropPayload( TAssetBrowserPanel::g_kPayloadMesh ) )
                TAssetBrowserPanel::applyDrop( p_ctx, *node, *payload );
            if ( const ImGuiPayload* payload = ImGui::AcceptDragDropPayload( TAssetBrowserPanel::g_kPayloadGpuAsset ) )
                TAssetBrowserPanel::applyDrop( p_ctx, *node, *payload );
            if ( const ImGuiPayload* payload = ImGui::AcceptDragDropPayload( TAssetBrowserPanel::g_kPayloadAudio ) )
                TAssetBrowserPanel::applyDrop( p_ctx, *node, *payload );
            if ( const ImGuiPayload* payload = ImGui::AcceptDragDropPayload( TAssetBrowserPanel::g_kPayloadTexture ) )
                TAssetBrowserPanel::applyDrop( p_ctx, *node, *payload );
            ImGui::EndDragDropTarget();
        }

        ImGui::End();
    }
}  // namespace Tomos
