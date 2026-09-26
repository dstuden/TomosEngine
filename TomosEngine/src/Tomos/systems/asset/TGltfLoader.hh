#pragma once

#include <cstdint>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <memory>
#include <string>
#include <vector>

#include "Tomos/core/scene/TSceneNode.hh"
#include "Tomos/gpu/TGpuEnums.hh"
#include "Tomos/systems/animation/TAnimationClip.hh"
#include "Tomos/systems/asset/TAssetSystem.hh"
#include "Tomos/util/math/TFrustum.hh"

namespace Tomos
{
    class TVkGpu;
    class TLevelStore;

    struct TLoadResult
    {
        TSceneNode*                m_root = nullptr;
        std::unique_ptr<TGpuAsset> m_asset;
    };

    // Host-only glTF package — no Vulkan. Safe to build on a worker thread.
    struct TCpuTextureData
    {
        uint32_t             m_width  = 0;
        uint32_t             m_height = 0;
        bool                 m_isSrgb = true;
        std::vector<uint8_t> m_pixels;  // RGBA8
    };

    // Texture slot: -1 = default white, -2 = missing checker, >=0 = package texture index.
    struct TCpuMaterialData
    {
        glm::vec4 m_baseColorFactor = glm::vec4( 1.0f );
        glm::vec3 m_emissionFactor  = glm::vec3( 0.0f );
        float     m_metallicFactor  = 0.0f;
        float     m_roughnessFactor = 1.0f;
        float     m_normalScale     = 1.0f;
        float     m_alphaCutoff     = 0.5f;
        TMatAlpha m_alphaMode       = TMatAlpha::Opaque;
        bool      m_doubleSided     = false;
        bool      m_hasNormalMap    = false;
        int32_t   m_baseTex         = -1;
        int32_t   m_metRghTex       = -1;
        int32_t   m_emissionTex     = -1;
        int32_t   m_normalTex       = -1;
    };

    struct TCpuMeshData
    {
        std::vector<glm::vec3>  m_positions;
        std::vector<glm::vec2>  m_uvs;
        std::vector<glm::vec3>  m_normals;
        std::vector<glm::vec4>  m_tangents;
        std::vector<glm::uvec4> m_joints;
        std::vector<glm::vec4>  m_weights;
        std::vector<uint16_t>   m_indices16;
        std::vector<uint32_t>   m_indices32;
        bool                    m_is16BitIndex = true;
        uint32_t                m_drawCount    = 0;
        TAABB                   m_aabb{};
        bool                    m_skinned = false;
        uint32_t                m_materialIndex = 0;
        std::vector<std::string> m_boneNames;
        std::vector<glm::mat4>   m_inverseBindMatrices;
    };

    struct TCpuNodeData
    {
        std::string          m_name;
        int32_t              m_parent = -1;
        glm::vec3            m_translation{ 0.0f };
        glm::quat            m_rotation{ 1.0f, 0.0f, 0.0f, 0.0f };
        glm::vec3            m_scale{ 1.0f };
        std::vector<uint32_t> m_meshIndices;
    };

    struct TCpuGltfPackage
    {
        std::string                      m_name;
        std::string                      m_sourcePath;
        std::string                      m_id;
        bool                             m_ok = false;
        std::string                      m_error;
        std::vector<TCpuMeshData>        m_meshes;
        std::vector<TCpuTextureData>     m_textures;
        std::vector<TCpuMaterialData>    m_materials;
        std::vector<TCpuNodeData>        m_nodes;
        std::vector<TAnimationClip>      m_clips;
    };

    class TGltfLoader
    {
    public:
        // Assimp + stb only — safe off the main thread.
        static TCpuGltfPackage decodeCpu( const std::string& p_path );

        // Main thread: GPU upload + nodes into p_store.
        static TLoadResult uploadGpu( TCpuGltfPackage&& p_package, TVkGpu& p_gpu, TLevelStore& p_store );

        static TLoadResult load( const std::string& p_path, TVkGpu& p_gpu, TLevelStore& p_store );
    };
}  // namespace Tomos
