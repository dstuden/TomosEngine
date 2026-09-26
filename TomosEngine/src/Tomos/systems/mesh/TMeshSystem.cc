#include "Tomos/systems/mesh/TMeshSystem.hh"

#include <algorithm>
#include <cstdint>
#include <tuple>
#include <vector>

#include "Tomos/core/scene/TScene.hh"
#include "Tomos/systems/camera/TCameraSystem.hh"
#include "Tomos/core/scene/TSceneNode.hh"
#include "Tomos/gpu/TGpuEnums.hh"
#include "Tomos/gpu/TRenderLimits.hh"
#include "Tomos/gpu/vulkan/TVkGpu.hh"
#include "Tomos/gpu/vulkan/TVkMaterial.hh"
#include "Tomos/gpu/vulkan/TVkMesh.hh"
#include "Tomos/gpu/vulkan/TVkPass.hh"
#include "Tomos/util/logger/TLogger.hh"
#include "Tomos/util/math/TFrustum.hh"
#include "Tomos/util/memory/TArenaAllocator.hh"
#include "Tomos/util/memory/TFrameAllocator.hh"

namespace Tomos
{
    namespace
    {
        struct TPendingDraw
        {
            const TVkMesh*     m_mesh     = nullptr;
            const TVkMaterial* m_material = nullptr;
            TInstanceData      m_instance{};
            bool               m_castShadow = true;
            bool               m_visible    = true;
            bool               m_blend      = false;
        };

        // Opaque/mask batch key.  Blend draws stay one-instance so the forward
        // pass can sort them back-to-front individually.
        auto batchKey( const TPendingDraw& p_d )
        {
            return std::tuple{ p_d.m_blend, reinterpret_cast<uintptr_t>( p_d.m_mesh ), reinterpret_cast<uintptr_t>( p_d.m_material ), p_d.m_castShadow,
                               p_d.m_visible };
        }

        bool meshIntersectsFrustum( const TMeshComponent& p_mc, const TSceneNode& p_node, const glm::vec4 p_planes[ 6 ] )
        {
            if ( p_mc.m_mesh == nullptr || !p_mc.m_mesh->m_aabb.valid() ) return true;
            const TAABB worldAabb = p_mc.m_mesh->m_aabb.transformed( p_node.m_transform.getGlobalMatrix() );
            return aabbIntersectsFrustum( worldAabb, p_planes );
        }
    }  // namespace

    void TMeshSystem::componentCreated( TSceneNode& p_node, TComponent& p_component )
    {
        auto& mc        = dynamic_cast<TMeshComponent&>( p_component );
        m_meshes[ &mc ] = &p_node;

        if ( auto* sk = dynamic_cast<TSkinnedMeshComponent*>( &mc ) ) m_skinned.push_back( sk );
    }

    void TMeshSystem::componentDestroyed( TSceneNode& /*p_node*/, TComponent& p_component )
    {
        auto& mc = dynamic_cast<TMeshComponent&>( p_component );
        m_meshes.erase( &mc );

        if ( auto* sk = dynamic_cast<TSkinnedMeshComponent*>( &mc ) ) m_skinned.erase( std::remove( m_skinned.begin(), m_skinned.end(), sk ), m_skinned.end() );
    }

    void TMeshSystem::lateUpdate( float /*p_dt*/ )
    {
        glm::vec4 planes[ 6 ];
        bool      haveFrustum = false;

        if ( m_scene != nullptr )
        {
            if ( auto* camSys = m_scene->ecs().maybeGetSystem<TCameraSystem>() )
            {
                if ( camSys->hasActiveCamera() )
                {
                    TSceneNode*       camNode = camSys->activeCameraNode();
                    TCameraComponent* cam     = camSys->activeCamera();
                    if ( camNode != nullptr && cam != nullptr && m_gpu != nullptr )
                    {
                        const glm::mat4 viewProj = cam->projMatrix( m_gpu->renderAspectRatio() ) * camNode->m_transform.getGlobalInvMatrix();
                        extractFrustumPlanes( viewProj, planes );
                        haveFrustum = true;
                    }
                }
            }
        }

        for ( auto* sk : m_skinned )
        {
            const auto  ownerIt = m_meshes.find( sk );
            TSceneNode* owner   = ownerIt != m_meshes.end() ? ownerIt->second : nullptr;

            if ( haveFrustum && !sk->m_castShadow && owner != nullptr && !meshIntersectsFrustum( *sk, *owner, planes ) ) continue;

            TLevelStore* store = owner != nullptr ? owner->store() : nullptr;
            for ( uint32_t i = 0; i < static_cast<uint32_t>( sk->m_joints.size() ); ++i )
            {
                TSkinJoint* joint     = &sk->m_joints[ i ];
                TSceneNode* jointNode = joint->resolve( store );
                if ( jointNode == nullptr ) continue;

                sk->m_boneMatrices[ i ] = jointNode->m_transform.getGlobalMatrix() * joint->m_inverseBindMtx;
            }
        }
    }

    const TVkMaterial* TMeshSystem::materialFor( const TMeshComponent& p_mc ) const
    {
        if ( p_mc.m_material == nullptr ) return nullptr;
        if ( p_mc.m_baseOverrideImage == nullptr && p_mc.m_emissionOverrideImage == nullptr ) return p_mc.m_material;

        const TOverrideKey key{ p_mc.m_material, p_mc.m_baseOverrideImage, p_mc.m_emissionOverrideImage };
        const auto         it = m_overrideMaterials.find( key );
        if ( it != m_overrideMaterials.end() ) return it->second.get();

        TVkGpu* gpu = m_gpu;
        if ( gpu == nullptr ) return p_mc.m_material;

        TVkMaterialDesc desc = p_mc.m_material->makeDesc( p_mc.m_baseOverrideImage, p_mc.m_emissionOverrideImage );
        auto            mat  = std::make_unique<TVkMaterial>( gpu->device(), gpu->physDevice(), gpu->descPool(), gpu->layouts().m_material, desc );
        const TVkMaterial* raw = mat.get();
        m_overrideMaterials.emplace( key, std::move( mat ) );
        return raw;
    }

    void TMeshSystem::populate( TFrameState& p_state ) const
    {
        TOMOS_HEAP_PROBE( "mesh.populate" );

        p_state.m_drawCalls.clear();
        p_state.m_instances.clear();
        p_state.m_bones.clear();
        p_state.m_meshesTotal  = 0;
        p_state.m_meshesCulled = 0;

        glm::vec4 planes[ 6 ];
        extractFrustumPlanes( p_state.m_viewProj, planes );

        TArena&                    arena = TFrameAllocator::get().arena();
        TArenaVector<TPendingDraw> pending{ TArenaAllocator<TPendingDraw>( arena ) };
        pending.reserve( m_meshes.size() );

        bool truncatedInstances = false;
        bool truncatedBones     = false;

        for ( const auto& [ mc, node ] : m_meshes )
        {
            if ( pending.size() >= g_kMaxInstances )
            {
                truncatedInstances = true;
                break;
            }

            const TVkMaterial* material = materialFor( *mc );
            if ( mc->m_mesh == nullptr || material == nullptr ) continue;
            ++p_state.m_meshesTotal;

            const glm::mat4& world = node->m_transform.getGlobalMatrix();

            bool visible = true;
            if ( mc->m_mesh->m_aabb.valid() ) visible = meshIntersectsFrustum( *mc, *node, planes );

            if ( !visible )
            {
                ++p_state.m_meshesCulled;
                if ( !mc->m_castShadow ) continue;
            }

            TPendingDraw item{};
            item.m_mesh       = mc->m_mesh;
            item.m_material   = material;
            item.m_castShadow = mc->m_castShadow;
            item.m_visible    = visible;
            item.m_blend      = ( material->alphaMode() == TMatAlpha::Blend );

            item.m_instance.m_transform    = world;
            item.m_instance.m_invTransform = node->m_transform.getGlobalInvMatrix();

            if ( auto* sk = dynamic_cast<TSkinnedMeshComponent*>( mc ) )
            {
                const auto count = static_cast<uint32_t>( sk->m_boneMatrices.size() );
                if ( count > 0 )
                {
                    if ( p_state.m_bones.size() + count <= g_kMaxBonesPerFrame )
                    {
                        item.m_instance.m_boneOffset = static_cast<uint32_t>( p_state.m_bones.size() );
                        item.m_instance.m_boneCount  = count;
                        p_state.m_bones.insert( p_state.m_bones.end(), sk->m_boneMatrices.begin(), sk->m_boneMatrices.end() );
                    }
                    else
                    {
                        truncatedBones = true;
                    }
                }
            }

            pending.push_back( item );
        }

        std::sort( pending.begin(), pending.end(), []( const TPendingDraw& p_a, const TPendingDraw& p_b ) { return batchKey( p_a ) < batchKey( p_b ); } );

        auto flush = [ & ]( uint32_t p_offset, uint32_t p_count, const TPendingDraw& p_proto )
        {
            if ( p_count == 0 ) return;
            TDrawCall dc{};
            dc.m_mesh           = p_proto.m_mesh;
            dc.m_material       = p_proto.m_material;
            dc.m_instanceOffset = p_offset;
            dc.m_instanceCount  = p_count;
            dc.m_castShadow     = p_proto.m_castShadow;
            dc.m_visible        = p_proto.m_visible;
            p_state.m_drawCalls.push_back( dc );
        };

        uint32_t     runOffset = 0;
        uint32_t     runCount  = 0;
        TPendingDraw runProto{};
        bool         haveRun = false;

        for ( const TPendingDraw& item : pending )
        {
            if ( p_state.m_instances.size() >= g_kMaxInstances )
            {
                truncatedInstances = true;
                break;
            }

            const auto instIndex = static_cast<uint32_t>( p_state.m_instances.size() );
            p_state.m_instances.push_back( item.m_instance );

            // Blend: never merge — transparent pass sorts per draw.
            if ( item.m_blend )
            {
                if ( haveRun )
                {
                    flush( runOffset, runCount, runProto );
                    haveRun  = false;
                    runCount = 0;
                }
                flush( instIndex, 1, item );
                continue;
            }

            if ( !haveRun )
            {
                haveRun   = true;
                runOffset = instIndex;
                runCount  = 1;
                runProto  = item;
                continue;
            }

            if ( batchKey( item ) == batchKey( runProto ) && instIndex == runOffset + runCount )
            {
                ++runCount;
                continue;
            }

            flush( runOffset, runCount, runProto );
            runOffset = instIndex;
            runCount  = 1;
            runProto  = item;
        }

        if ( haveRun ) flush( runOffset, runCount, runProto );

        if ( truncatedInstances ) TLOG_WARN() << "[TMeshSystem] Instance cap reached (" << g_kMaxInstances << ") — dropping remaining meshes";
        if ( truncatedBones ) TLOG_WARN() << "[TMeshSystem] Bone palette full (" << g_kMaxBonesPerFrame << ") — skinned meshes drawn without bones";
    }
}  // namespace Tomos
