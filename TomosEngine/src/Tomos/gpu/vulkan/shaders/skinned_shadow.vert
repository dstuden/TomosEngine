#version 450

// Depth-only skinned shadow pass.
// Bone matrices are jointWorld * inverseBind — already world-space.

layout( push_constant ) uniform Push
{
    mat4 lightVP;
} pc;

#include "common/instance.glsl"

layout( set = 0, binding = 1 ) readonly buffer InstanceSSBO
{
    Instance instances[];
};

layout( set = 0, binding = 6 ) readonly buffer BoneSSBO
{
    mat4 bones[];
};

layout( location = 0 ) in vec3 inPosition;
layout( location = 1 ) in uvec4 inJoints;
layout( location = 2 ) in vec4 inWeights;

void main()
{
    const Instance inst = instances[ gl_InstanceIndex ];

    mat4 skin =
            inWeights.x * bones[ inst.boneOffset + inJoints.x ] +
            inWeights.y * bones[ inst.boneOffset + inJoints.y ] +
            inWeights.z * bones[ inst.boneOffset + inJoints.z ] +
            inWeights.w * bones[ inst.boneOffset + inJoints.w ];

    gl_Position = pc.lightVP * skin * vec4( inPosition, 1.0 );
}
