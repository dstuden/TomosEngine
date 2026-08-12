#version 450

layout( set = 0, binding = 0 ) uniform sampler2D tHdr;
layout( set = 0, binding = 1 ) uniform sampler2D tAO;

layout( location = 0 ) in vec2 vUV;
layout( location = 0 ) out vec4 outColor;

void main()
{
    const vec3  hdr = texture( tHdr, vUV ).rgb;
    const float ao  = texture( tAO, vUV ).r;
    outColor = vec4( hdr * ao, 1.0 );
}
