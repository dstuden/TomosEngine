// Must stay in sync with Tomos::TEmitterData (TVkPass.hh), std430 (128 B).

struct Emitter
{
    vec3  position;
    float gravity;
    vec3  velocityMin;
    float lifetimeMin;
    vec3  velocityMax;
    float lifetimeMax;
    vec4  colorStart;
    vec4  colorEnd;
    vec2  sizeStart;
    vec2  sizeEnd;
    uint  spawnCount;
    uint  seed;
    uint  texIndex;
    uint  _pad0;
    vec2  uvMin;
    vec2  uvMax;
};
