// Must stay in sync with Tomos::TLightData (TVkPass.hh), std430.

struct Light
{
    vec3  position;
    float _pad0;
    vec3  direction;
    float _pad1;
    vec3  color;
    uint  type;  // 0 point, 1 directional, 2 spot
    float intensity;
    float maxRange;
    float innerCone;  // cos(inner half-angle)
    float outerCone;  // cos(outer half-angle)
    int   shadowMap;  // base layer, or -1 (point lights use base..base+5)
    float _pad2;
    float _pad3;
    float _pad4;
    mat4  vp;  // spot/dir shadow VP (unused for point)
};

struct LightCell
{
    uint offset;
    uint count;
};
