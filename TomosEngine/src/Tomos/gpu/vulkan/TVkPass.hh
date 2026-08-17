#pragma once

// CPU ↔ GPU frame types — layouts must match shader std140/std430.

#include <cstddef>
#include <cstdint>
#include <glm/glm.hpp>
#include <vector>

#include "Tomos/gpu/TBillboardMode.hh"
#include "Tomos/gpu/TRenderLimits.hh"

namespace Tomos
{
    class TVkMesh;
    class TVkMaterial;
    class TVkImage;

    // Debug visualizations rendered by forward.frag (TFrameState::m_debugMode).
    enum class TDebugView : uint32_t
    {
        None         = 0,
        ClusterGrid  = 1,  // hashed color per froxel — shows the cluster frustums
        LightHeatmap = 2,  // lights-per-cluster heatmap (blue → green → red)
        DepthSlices  = 3,  // color by logarithmic Z slice only
    };

    // set0 binding0; match shader std140.
    struct alignas( 16 ) TSceneUBO
    {
        glm::mat4  m_viewProj;
        glm::mat4  m_view;
        glm::mat4  m_viewInv;
        glm::mat4  m_projInv;
        glm::vec4  m_cameraPosNear;  // xyz = camera world pos, w = near plane
        glm::vec4  m_screenFar;  // xy = viewport size in px, z = far plane, w = light count
        glm::uvec4 m_clusterGrid;  // xyz = cluster grid dims, w = max lights per cluster
        glm::uvec4 m_debug;  // x = TDebugView, yzw unused
        float      m_time{ 0.0f };  // sim elapsed seconds (animated shaders)
        float      m_padTime[ 3 ]{ 0.0f, 0.0f, 0.0f };
    };

    struct alignas( 16 ) TInstanceData
    {
        glm::mat4 m_transform{};
        glm::mat4 m_invTransform{};
        uint32_t  m_boneOffset{ 0 };  // into TFrameState::m_bones (skinned only)
        uint32_t  m_boneCount{ 0 };  // 0 = static mesh
        uint32_t  m_pad0{ 0 };
        uint32_t  m_pad1{ 0 };
    };

    struct alignas( 16 ) TLightData
    {
        glm::vec3 m_position;
        float     m_pad0{ 0.0f };
        glm::vec3 m_direction;
        float     m_pad1{ 0.0f };
        glm::vec3 m_color;
        uint32_t  m_type;  // 0 = point, 1 = directional, 2 = spot
        float     m_intensity;
        float     m_maxRange;
        float     m_innerCone;  // cos(inner half-angle)
        float     m_outerCone;  // cos(outer half-angle)
        int32_t   m_shadowMap;  // base layer in the shadow-map array, or -1 (point: +0..+5 faces)
        float     m_pad2[ 3 ]{ 0.0f, 0.0f, 0.0f };
        glm::mat4 m_vp;  // light view-projection for spot/dir shadow lookup (unused for point)
    };

    struct TLightCell
    {
        uint32_t m_offset;
        uint32_t m_count;
    };

    struct TDrawCall
    {
        const TVkMesh*     m_mesh{};
        const TVkMaterial* m_material{};
        uint32_t           m_instanceOffset{};
        uint32_t           m_instanceCount{};
        bool               m_castShadow{};
        // False when the camera frustum culled this draw — skipped by the
        // forward pass but still rendered into shadow maps (off-screen
        // geometry must keep casting shadows into the visible scene).
        bool m_visible = true;
    };

    struct alignas( 16 ) TSpriteData
    {
        glm::vec3 m_position;
        float     m_rotation;
        glm::vec2 m_size;
        glm::vec2 m_uvMin;
        glm::vec2 m_uvMax;
        uint32_t  m_mode;  // TBillboardMode
        uint32_t  m_pad0{ 0 };
        glm::vec4 m_color;
    };

    struct TSpriteBatch
    {
        const TVkImage* m_texture;
        uint32_t        m_offset;
        uint32_t        m_count;
    };

    // GPU particle (persistent pool; written by particle_sim.comp, read by particle.vert).
    struct alignas( 16 ) TParticleData
    {
        glm::vec3 m_position{};
        float     m_life{};  // remaining seconds; <= 0 = dead / on free list
        glm::vec3 m_velocity{};
        float     m_maxLife{};
        glm::vec2 m_sizeStart{};
        glm::vec2 m_sizeEnd{};
        glm::vec4 m_colorStart{};
        glm::vec4 m_colorEnd{};
        float     m_rotation{};
        float     m_gravity{};
        uint32_t  m_texIndex{ 0 };  // into TFrameState::m_particleTextures (stable)
        uint32_t  m_pad0{ 0 };
        glm::vec2 m_uvMin{ 0.0f, 0.0f };
        glm::vec2 m_uvMax{ 1.0f, 1.0f };
    };
    static_assert( sizeof( TParticleData ) == 112, "TParticleData must match particle_*.glsl std430" );
    static_assert( offsetof( TParticleData, m_texIndex ) == 88, "TParticleData::m_texIndex offset" );

    // Spawn count is decided on the CPU (rate * dt + burst); the GPU allocates slots.
    struct alignas( 16 ) TEmitterData
    {
        glm::vec3 m_position;
        float     m_gravity;
        glm::vec3 m_velocityMin;
        float     m_lifetimeMin;
        glm::vec3 m_velocityMax;
        float     m_lifetimeMax;
        glm::vec4 m_colorStart;
        glm::vec4 m_colorEnd;
        glm::vec2 m_sizeStart;
        glm::vec2 m_sizeEnd;
        uint32_t  m_spawnCount{ 0 };
        uint32_t  m_seed{ 0 };
        uint32_t  m_texIndex{ 0 };
        uint32_t  m_pad0{ 0 };
        glm::vec2 m_uvMin{ 0.0f, 0.0f };
        glm::vec2 m_uvMax{ 1.0f, 1.0f };
    };
    static_assert( sizeof( TEmitterData ) == 128, "TEmitterData must match particle_sim.comp std430" );
    static_assert( offsetof( TEmitterData, m_texIndex ) == 104, "TEmitterData::m_texIndex offset" );

    // Particle free-list + indirect draw args (GPU). Layout must match particle_sim.comp.
    struct alignas( 16 ) TParticleCounters
    {
        uint32_t m_freeCount{ 0 };
        uint32_t m_aliveCount{ 0 };
        uint32_t m_pad0{ 0 };
        uint32_t m_pad1{ 0 };
        // VkDrawIndirectCommand
        uint32_t m_vertexCount{ 6 };
        uint32_t m_instanceCount{ 0 };
        uint32_t m_firstVertex{ 0 };
        uint32_t m_firstInstance{ 0 };
    };

    struct alignas( 16 ) TParticleSimUBO
    {
        float    m_dt{ 0.0f };
        uint32_t m_emitterCount{ 0 };
        uint32_t m_maxParticles{ g_kMaxParticles };
        uint32_t m_frameIndex{ 0 };
    };

    struct TFrameState
    {
        // Identity until the first TCameraSystem::populate — consumers
        // (frustum culling, UBO upload) must not read garbage on frame 0.
        glm::mat4                  m_viewProj{ 1.0f };
        glm::mat4                  m_view{ 1.0f };
        glm::mat4                  m_viewInv{ 1.0f };
        glm::mat4                  m_viewProjInv{ 1.0f };
        glm::mat4                  m_proj{ 1.0f };
        glm::mat4                  m_projInv{ 1.0f };
        float                      m_near{ 0.1f };
        float                      m_far{ 1000.0f };
        float                      m_time{ 0.0f };  // sim elapsed (TTime::elapsed)
        float                      m_particleDt{ 0.0f };  // sim step for GPU particles
        TDebugView                 m_debugMode{ TDebugView::None };
        uint32_t                   m_meshesTotal{ 0 };  // before frustum cull
        uint32_t                   m_meshesCulled{ 0 };  // rejected by frustum
        std::vector<TDrawCall>     m_drawCalls;
        std::vector<TInstanceData> m_instances;
        std::vector<glm::mat4>     m_bones;  // flattened bone palettes for skinned draws
        std::vector<TLightData>    m_lights;
        std::vector<TSpriteData>   m_sprites;
        std::vector<TSpriteBatch>  m_spriteBatches;
        std::vector<TEmitterData>  m_emitters;
        // Stable particle texture slots (index 0 = nullptr → default white).
        // Copied from TParticleSystem each frame for the additive draw binds.
        std::vector<const TVkImage*> m_particleTextures;
    };
}  // namespace Tomos
