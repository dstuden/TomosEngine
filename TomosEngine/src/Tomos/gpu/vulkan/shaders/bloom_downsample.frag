#version 450

layout( set = 0, binding = 0 ) uniform sampler2D tSrc;

layout( push_constant ) uniform Push
{
    vec2 halfPixel;  // 0.5 / sourceSize
    vec2 _pad;
} pc;

layout( location = 0 ) in vec2 vUV;
layout( location = 0 ) out vec4 outColor;

// Dual Kawase downsample (Call of Duty style 5-tap diamond).
void main()
{
    const vec2 hp = pc.halfPixel;

    vec3 sum = texture( tSrc, vUV ).rgb * 4.0;
    sum += texture( tSrc, vUV - hp ).rgb;
    sum += texture( tSrc, vUV + hp ).rgb;
    sum += texture( tSrc, vUV + vec2( hp.x, -hp.y ) ).rgb;
    sum += texture( tSrc, vUV - vec2( hp.x, -hp.y ) ).rgb;

    outColor = vec4( sum * 0.125, 1.0 );
}
