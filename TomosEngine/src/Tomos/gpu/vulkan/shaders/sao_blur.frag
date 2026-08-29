#version 450

layout( set = 0, binding = 0 ) uniform sampler2D tAO;
layout( set = 0, binding = 1 ) uniform sampler2D tLinearZ;

layout( push_constant ) uniform Push
{
    vec2  texelSize;   // 1/width, 1/height
    float horizontal;  // 1 = H, 0 = V
    float sharpness;   // depth edge sharpness (lower = softer blur on flats)
} pc;

layout( location = 0 ) in vec2 vUV;
layout( location = 0 ) out float outAO;

// 7-tap Gaussian, stride 2 (covers ±2..12 px after micro-filter).
const float k_w[ 4 ] = float[]( 0.2941176, 0.1764706, 0.0882353, 0.0294118 );
const float k_stride = 2.0;

void main()
{
    const float ao0 = texture( tAO, vUV ).r;
    const float z0  = texture( tLinearZ, vUV ).r;

    const vec2 dir = pc.horizontal > 0.5 ? vec2( pc.texelSize.x, 0.0 ) : vec2( 0.0, pc.texelSize.y );

    float aoSum = ao0 * k_w[ 0 ];
    float wSum  = k_w[ 0 ];

    for ( int i = 1; i <= 3; ++i )
    {
        const float r   = float( i ) * k_stride;
        const vec2  off = dir * r;

        const float aoL = texture( tAO, vUV - off ).r;
        const float aoR = texture( tAO, vUV + off ).r;
        const float zL  = texture( tLinearZ, vUV - off ).r;
        const float zR  = texture( tLinearZ, vUV + off ).r;

        const float dL = ( zL - z0 ) * pc.sharpness;
        const float dR = ( zR - z0 ) * pc.sharpness;
        const float wL = k_w[ i ] * exp2( -dL * dL );
        const float wR = k_w[ i ] * exp2( -dR * dR );

        aoSum += aoL * wL + aoR * wR;
        wSum += wL + wR;
    }

    outAO = aoSum / max( wSum, 1e-6 );
}
