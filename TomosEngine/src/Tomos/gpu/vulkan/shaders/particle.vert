#version 450

// Additive particle billboards — spherical, vertex-pulled from the GPU particle pool.
// Push constant selects which texture batch to draw; mismatched particles are
// clipped in the vertex shader (and discarded in the fragment shader).

layout( set = 0, binding = 0 ) uniform SceneUBO
{
#include "common/scene_ubo.glsl"
} scene;

#include "common/particle.glsl"

layout( set = 0, binding = 1 ) readonly buffer ParticleSSBO
{
    Particle particles[];
};

layout( set = 0, binding = 2 ) readonly buffer DrawIndexSSBO
{
    uint drawIndices[];
};

layout( push_constant ) uniform Push
{
    uint texIndex;
} pc;

layout( location = 0 ) out vec2  vUV;
layout( location = 1 ) out vec2  vLocalUV;
layout( location = 2 ) out vec4  vColor;
layout( location = 3 ) flat out uint vTexIndex;

#include "common/quad.glsl"

void main()
{
    const uint     pidx = drawIndices[ gl_InstanceIndex ];
    const Particle p    = particles[ pidx ];

    vTexIndex = p.texIndex;

    // Wrong texture batch — push outside the clip volume (no fragment work).
    if ( p.texIndex != pc.texIndex )
    {
        vUV         = vec2( 0.0 );
        vLocalUV    = vec2( 0.0 );
        vColor      = vec4( 0.0 );
        gl_Position = vec4( 2.0, 2.0, 2.0, 1.0 );
        return;
    }

    const float age   = 1.0 - clamp( p.life / max( p.maxLife, 1e-4 ), 0.0, 1.0 );
    const vec2  size  = mix( p.sizeStart, p.sizeEnd, age );
    const vec4  color = mix( p.colorStart, p.colorEnd, age );

    vec2 corner = k_corners[ gl_VertexIndex ];
    const float c = cos( p.rotation );
    const float s = sin( p.rotation );
    corner = vec2( corner.x * c - corner.y * s, corner.x * s + corner.y * c );
    corner *= size;

    const vec3 right = normalize( vec3( scene.viewInv[ 0 ] ) );
    const vec3 up    = normalize( vec3( scene.viewInv[ 1 ] ) );

    const vec3 worldPos = p.position + corner.x * right + corner.y * up;

    const vec2 t = k_corners[ gl_VertexIndex ] + 0.5;
    vLocalUV     = t;
    vUV          = mix( p.uvMin, p.uvMax, vec2( t.x, 1.0 - t.y ) );
    vColor       = color;

    gl_Position = scene.viewProj * vec4( worldPos, 1.0 );
}
