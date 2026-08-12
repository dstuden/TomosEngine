// Must stay in sync with Tomos::TInstanceData (TVkPass.hh), std430.

struct Instance
{
    mat4 transform;
    mat4 invTransform;
    uint boneOffset;
    uint boneCount;  // CPU-side; unused in GLSL
    uint _pad0;
    uint _pad1;
};
