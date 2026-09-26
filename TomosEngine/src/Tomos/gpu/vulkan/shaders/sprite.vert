#version 450

// World-space sprite / billboard pass.
// No vertex buffers: gl_VertexIndex selects a quad corner, gl_InstanceIndex
// selects the sprite in the SSBO.  Orientation depends on the billboard mode.

layout( set = 0, binding = 0 ) uniform SceneUBO
{
#include "common/scene_ubo.glsl"
} scene;

struct Sprite
{
    vec3  position;
    float rotation;
    vec2  size;
    vec2  uvMin;
    vec2  uvMax;
    uint  mode;  // 0 spherical, 1 cylindrical, 2 fixed
    float alphaCutoff;
    vec4  color;
};

layout( set = 0, binding = 1 ) readonly buffer SpriteSSBO
{
    Sprite sprites[];
};

layout( location = 0 ) out vec2 vUV;
layout( location = 1 ) out vec4 vColor;
layout( location = 2 ) flat out float vAlphaCutoff;

#include "common/quad.glsl"

void main()
{
    const Sprite spr    = sprites[ gl_InstanceIndex ];
    vec2         corner = k_corners[ gl_VertexIndex ];

    const float c = cos( spr.rotation );
    const float s = sin( spr.rotation );
    corner = vec2( corner.x * c - corner.y * s, corner.x * s + corner.y * c );
    corner *= spr.size;

    vec3 right;
    vec3 up;
    if ( spr.mode == 0u )  // spherical — camera basis
    {
        right = normalize( vec3( scene.viewInv[ 0 ] ) );
        up    = normalize( vec3( scene.viewInv[ 1 ] ) );
    }
    else if ( spr.mode == 1u )  // cylindrical — rotate around world Y
    {
        const vec3 camRight = vec3( scene.viewInv[ 0 ] );
        right               = normalize( vec3( camRight.x, 0.0, camRight.z ) );
        up                  = vec3( 0.0, 1.0, 0.0 );
    }
    else  // fixed — world XY plane
    {
        right = vec3( 1.0, 0.0, 0.0 );
        up    = vec3( 0.0, 1.0, 0.0 );
    }

    const vec3 worldPos = spr.position + corner.x * right + corner.y * up;

    // UV: corner (-0.5..0.5) → (0..1), Y flipped so uvMin is top-left.
    const vec2 t = k_corners[ gl_VertexIndex ] + 0.5;
    vUV          = mix( spr.uvMin, spr.uvMax, vec2( t.x, 1.0 - t.y ) );
    vColor       = spr.color;
    vAlphaCutoff = spr.alphaCutoff;

    gl_Position = scene.viewProj * vec4( worldPos, 1.0 );
}
