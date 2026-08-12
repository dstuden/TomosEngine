#version 450

// Unlit alpha-blended sprite shading.

layout( set = 1, binding = 0 ) uniform sampler2D tSprite;

layout( location = 0 ) in vec2 vUV;
layout( location = 1 ) in vec4 vColor;

layout( location = 0 ) out vec4 outColor;

const float k_alphaDiscard = 0.01;

void main()
{
    const vec4 texel = texture( tSprite, vUV ) * vColor;

    // Skip nearly invisible fragments — cheaper than sorting for sparse sprites.
    if ( texel.a < k_alphaDiscard ) discard;

    outColor = texel;
}
