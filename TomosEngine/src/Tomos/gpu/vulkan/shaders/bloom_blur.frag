#version 450

layout( set = 0, binding = 0 ) uniform sampler2D tSrc;

layout( push_constant ) uniform Push
{
    vec2  texelSize;  // 1/width, 1/height of source
    float horizontal; // 1 = H blur, 0 = V blur
    float _pad;
} pc;

layout( location = 0 ) in vec2 vUV;
layout( location = 0 ) out vec4 outColor;

// Separable 9-tap Gaussian (center + 4 offsets each side).
const float k_w[ 5 ] = float[]( 0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216 );

void main()
{
    const vec2 dir = pc.horizontal > 0.5 ? vec2( pc.texelSize.x, 0.0 ) : vec2( 0.0, pc.texelSize.y );

    vec3 result = texture( tSrc, vUV ).rgb * k_w[ 0 ];
    for ( int i = 1; i < 5; ++i )
    {
        result += texture( tSrc, vUV + dir * float( i ) ).rgb * k_w[ i ];
        result += texture( tSrc, vUV - dir * float( i ) ).rgb * k_w[ i ];
    }
    outColor = vec4( result, 1.0 );
}
