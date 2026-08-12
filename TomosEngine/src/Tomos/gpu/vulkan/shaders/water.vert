#version 450

// Same outputs as forward.vert, plus light Y displacement.

layout( set = 0, binding = 0 ) uniform SceneUBO
{
#include "common/scene_ubo.glsl"
} scene;

#include "common/instance.glsl"

layout( set = 0, binding = 1 ) readonly buffer InstanceSSBO
{
    Instance instances[];
};

layout( location = 0 ) in vec3 inPosition;
layout( location = 1 ) in vec2 inUV;
layout( location = 2 ) in vec3 inNormal;
layout( location = 3 ) in vec4 inTangent;

layout( location = 0 ) out vec2 vUV;
layout( location = 1 ) out vec3 vWorldPos;
layout( location = 2 ) out vec3 vNormal;
layout( location = 3 ) out float vViewZ;
layout( location = 4 ) out vec4 vTangent;

void main()
{
    const Instance inst = instances[ gl_InstanceIndex ];

    vec3 localPos = inPosition;
    // Two overlapping sine waves in object space (scaled planes stay coherent).
    const float t = scene.time;
    localPos.y += 0.02 * sin( localPos.x * 6.0 + t * 1.4 ) + 0.015 * sin( localPos.z * 5.0 - t * 1.1 );

    const vec4 worldPos = inst.transform * vec4( localPos, 1.0 );

    const mat3 normalMatrix = transpose( mat3( inst.invTransform ) );
    vNormal   = normalize( normalMatrix * inNormal );
    vTangent  = vec4( normalize( normalMatrix * inTangent.xyz ), inTangent.w );
    vWorldPos = worldPos.xyz;
    vUV       = inUV;
    vViewZ    = -( scene.view * worldPos ).z;

    gl_Position = scene.viewProj * worldPos;
}
