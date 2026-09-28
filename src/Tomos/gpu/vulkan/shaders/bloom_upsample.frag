#version 450

layout( set = 0, binding = 0 ) uniform sampler2D tSrc;

layout( push_constant ) uniform Push
{
    vec2 halfPixel;  // 0.5 / sourceSize
    vec2 _pad;
} pc;

layout( location = 0 ) in vec2 vUV;
layout( location = 0 ) out vec4 outColor;

// Dual Kawase upsample (9-tap filtered diamond).
void main()
{
    const vec2 hp = pc.halfPixel;

    vec3 sum = texture( tSrc, vUV + vec2( -hp.x * 2.0, 0.0 ) ).rgb;
    sum += texture( tSrc, vUV + vec2( -hp.x, hp.y ) ).rgb * 2.0;
    sum += texture( tSrc, vUV + vec2( 0.0, hp.y * 2.0 ) ).rgb;
    sum += texture( tSrc, vUV + vec2( hp.x, hp.y ) ).rgb * 2.0;
    sum += texture( tSrc, vUV + vec2( hp.x * 2.0, 0.0 ) ).rgb;
    sum += texture( tSrc, vUV + vec2( hp.x, -hp.y ) ).rgb * 2.0;
    sum += texture( tSrc, vUV + vec2( 0.0, -hp.y * 2.0 ) ).rgb;
    sum += texture( tSrc, vUV + vec2( -hp.x, -hp.y ) ).rgb * 2.0;

    outColor = vec4( sum * ( 1.0 / 12.0 ), 1.0 );
}
