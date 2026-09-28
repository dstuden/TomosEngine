#version 450

layout( set = 0, binding = 0 ) uniform SceneUBO
{
#include "common/scene_ubo.glsl"
} scene;

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
layout( location = 1 ) in vec2 inUV;
layout( location = 2 ) in vec3 inNormal;
layout( location = 3 ) in vec4 inTangent;
layout( location = 4 ) in uvec4 inJoints;
layout( location = 5 ) in vec4 inWeights;

layout( location = 0 ) out vec2 vUV;
layout( location = 1 ) out vec3 vWorldPos;
layout( location = 2 ) out vec3 vNormal;
layout( location = 3 ) out float vViewZ;
layout( location = 4 ) out vec4 vTangent;

void main()
{
    const Instance inst = instances[ gl_InstanceIndex ];

    // Bone matrices are jointWorld * inverseBind — already world-space.
    mat4 skin =
            inWeights.x * bones[ inst.boneOffset + inJoints.x ] +
            inWeights.y * bones[ inst.boneOffset + inJoints.y ] +
            inWeights.z * bones[ inst.boneOffset + inJoints.z ] +
            inWeights.w * bones[ inst.boneOffset + inJoints.w ];

    const vec4 worldPos = skin * vec4( inPosition, 1.0 );
    const mat3 skinN    = mat3( skin );

    vNormal   = normalize( skinN * inNormal );
    vTangent  = vec4( normalize( skinN * inTangent.xyz ), inTangent.w );
    vWorldPos = worldPos.xyz;
    vUV       = inUV;
    vViewZ    = -( scene.view * worldPos ).z;

    gl_Position = scene.viewProj * worldPos;
}
