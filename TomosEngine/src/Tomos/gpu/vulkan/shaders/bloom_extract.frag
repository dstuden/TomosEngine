#version 450

layout( set = 0, binding = 0 ) uniform sampler2D tHdr;

layout( push_constant ) uniform Push
{
    float threshold;
    float _pad0;
    float _pad1;
    float _pad2;
} pc;

layout( location = 0 ) in vec2 vUV;
layout( location = 0 ) out vec4 outColor;

void main()
{
    const vec3  c   = texture( tHdr, vUV ).rgb;
    const float lum = dot( c, vec3( 0.2126, 0.7152, 0.0722 ) );  // Rec.709
    const float soft = max( lum - pc.threshold, 0.0 );
    outColor = vec4( c * ( soft / max( lum, 1e-4 ) ), 1.0 );
}
