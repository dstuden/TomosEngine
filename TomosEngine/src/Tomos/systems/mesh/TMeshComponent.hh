#pragma once

#include <glm/glm.hpp>
#include <memory>
#include <vector>

#include "Tomos/core/scene/TSceneNode.hh"
#include "Tomos/systems/TComponent.hh"
#include "Tomos/systems/asset/TAssetHandles.hh"

namespace Tomos
{
    struct TVkMesh;
    class TVkMaterial;
    class TAssetSystem;

    // Borrowed mesh/material from TAssetSystem (see TAssetHandles.hh).
    class TMeshComponent : public TComponent
    {
    public:
        TMeshComponent( const TVkMesh* p_mesh, const TVkMaterial* p_material, bool p_castShadow = true ) :
            m_mesh( p_mesh ), m_material( p_material ), m_castShadow( p_castShadow )
        {
        }

        TMeshComponent( TMeshAssetRef p_ref, const TVkMesh* p_mesh, const TVkMaterial* p_material, TResourceGeneration p_generation,
                        bool p_castShadow = true ) :
            m_ref( std::move( p_ref ) ), m_boundGeneration( p_generation ), m_mesh( p_mesh ), m_material( p_material ), m_castShadow( p_castShadow )
        {
        }

        TMeshAssetRef       m_ref{};
        TResourceGeneration m_boundGeneration = 0;

        const TVkMesh*     m_mesh       = nullptr;
        const TVkMaterial* m_material   = nullptr;
        bool               m_castShadow = true;

        bool rebind( const TAssetSystem& p_assets );
    };

    struct TSkinJoint
    {
        std::weak_ptr<TSceneNode> m_node;
        glm::mat4                 m_inverseBindMtx;
    };

    // Bone matrices → TFrameState::m_bones via TMeshSystem::lateUpdate().
    class TSkinnedMeshComponent : public TMeshComponent
    {
    public:
        TSkinnedMeshComponent( const TVkMesh* p_mesh, const TVkMaterial* p_material, std::vector<TSkinJoint> p_joints ) :
            TMeshComponent( p_mesh, p_material, true ), m_joints( std::move( p_joints ) )
        {
            m_boneMatrices.resize( m_joints.size(), glm::mat4( 1.0f ) );
        }

        TSkinnedMeshComponent( TMeshAssetRef p_ref, const TVkMesh* p_mesh, const TVkMaterial* p_material, TResourceGeneration p_generation,
                               std::vector<TSkinJoint> p_joints ) :
            TMeshComponent( std::move( p_ref ), p_mesh, p_material, p_generation, true ), m_joints( std::move( p_joints ) )
        {
            m_boneMatrices.resize( m_joints.size(), glm::mat4( 1.0f ) );
        }

        std::vector<TSkinJoint> m_joints;
        std::vector<glm::mat4>  m_boneMatrices;
    };
}  // namespace Tomos
