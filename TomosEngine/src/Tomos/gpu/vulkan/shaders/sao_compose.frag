#version 450

layout( set = 0, binding = 0 ) uniform sampler2D tHdr;
layout( set = 0, binding = 1 ) uniform sampler2D tAO;

layout( push_constant ) uniform Push
{
    float intensity;
    float _pad0;
    float _pad1;
    float _pad2;
} pc;

layout( location = 0 ) in vec2 vUV;
layout( location = 0 ) out vec4 outColor;

void main()
{
    const vec3  hdr = texture( tHdr, vUV ).rgb;
    const float ao  = texture( tAO, vUV ).r;
    const float factor = mix( 1.0, ao, pc.intensity );
    outColor = vec4( hdr * factor, 1.0 );
}
