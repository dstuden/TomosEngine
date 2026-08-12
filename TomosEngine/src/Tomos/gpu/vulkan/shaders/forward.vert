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

layout( location = 0 ) in vec3 inPosition;
layout( location = 1 ) in vec2 inUV;
layout( location = 2 ) in vec3 inNormal;
layout( location = 3 ) in vec4 inTangent;  // xyz = tangent, w = bitangent sign

layout( location = 0 ) out vec2 vUV;
layout( location = 1 ) out vec3 vWorldPos;
layout( location = 2 ) out vec3 vNormal;
layout( location = 3 ) out float vViewZ;
layout( location = 4 ) out vec4 vTangent;  // world-space tangent.xyz + sign.w

void main()
{
    const Instance inst     = instances[ gl_InstanceIndex ];
    const vec4     worldPos = inst.transform * vec4( inPosition, 1.0 );

    // normalMatrix = transpose(inverse(mat3(model))) == transpose(mat3(modelInv))
    const mat3 normalMatrix = transpose( mat3( inst.invTransform ) );
    vNormal   = normalize( normalMatrix * inNormal );
    vTangent  = vec4( normalize( normalMatrix * inTangent.xyz ), inTangent.w );
    vWorldPos = worldPos.xyz;
    vUV       = inUV;
    vViewZ    = -( scene.view * worldPos ).z;  // positive distance along view dir

    gl_Position = scene.viewProj * worldPos;
}
