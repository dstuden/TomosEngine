// Must stay in sync with Tomos::TParticleTexBuckets (TVkPass.hh) and
// g_kMaxParticleTextures (TRenderLimits.hh), std430.

#define PARTICLE_TEX_SLOTS 64

struct TexBuckets
{
    uint counts[ PARTICLE_TEX_SLOTS ];   // alive particles per texture slot
    uint offsets[ PARTICLE_TEX_SLOTS ];  // exclusive prefix sum of counts
    uint writeCursor[ PARTICLE_TEX_SLOTS ];
};
