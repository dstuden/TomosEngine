// Must stay in sync with Tomos::TParticleData (TVkPass.hh), std430 (112 B).

struct Particle
{
    vec3  position;
    float life;
    vec3  velocity;
    float maxLife;
    vec2  sizeStart;
    vec2  sizeEnd;
    vec4  colorStart;
    vec4  colorEnd;
    float rotation;
    float gravity;
    uint  texIndex;
    uint  _pad0;  // aligns uvMin to 8 bytes
    vec2  uvMin;
    vec2  uvMax;
};
