#version 450

// Shared by cutout and blend sprite pipelines; discard below vAlphaCutoff.

layout( set = 1, binding = 0 ) uniform sampler2D tSprite;

layout( location = 0 ) in vec2  vUV;
layout( location = 1 ) in vec4  vColor;
layout( location = 2 ) flat in float vAlphaCutoff;

layout( location = 0 ) out vec4 outColor;

void main()
{
    const vec4 texel = texture( tSprite, vUV ) * vColor;

    if ( texel.a < vAlphaCutoff ) discard;

    outColor = texel;
}
