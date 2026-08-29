// McGuire / Mara / Luebke — Scalable Ambient Obscurance (HPG 2012), core variant.
// Finite far plane (Tomos perspectiveVk). Infinite-far projection would improve Z precision.

const float k_saoSkyDepth = 0.99999;
const float k_saoEpsilon  = 0.01;   // meters
const float k_saoSigma    = 2.25;
const float k_saoZmin     = -200.0; // farthest plane for sharp AO (camera Z < 0)
const float k_saoTurns    = 7.0;    // spiral turns for s ≈ 9

// Hyperbolic depth d ∈ [0,1] → camera-space Z < 0 (finite near/far distances > 0).
float saoLinearZ( float p_d, float p_near, float p_far )
{
    // zn = -near, zf = -far → c = [zn·zf, zn−zf, zf]
    return ( p_near * p_far ) / ( p_d * ( p_far - p_near ) - p_far );
}

// Reconstruct camera-space position from integer pixel + linear Z (paper eq. 3).
// P is column-major projection (Vulkan Y-flip already baked into P[1][1]).
vec3 saoReconstructCS( vec2 p_pixel, float p_z, mat4 p_proj, vec2 p_screenSize )
{
    const float p00 = p_proj[ 0 ][ 0 ];
    const float p11 = p_proj[ 1 ][ 1 ];
    const float p02 = p_proj[ 2 ][ 0 ];
    const float p12 = p_proj[ 2 ][ 1 ];

    const float x = p_z * ( ( 1.0 - p02 ) / p00 - 2.0 * ( p_pixel.x + 0.5 ) / ( p_screenSize.x * p00 ) );
    const float y = p_z * ( ( 1.0 + p12 ) / p11 + 2.0 * ( p_pixel.y + 0.5 ) / ( p_screenSize.y * p11 ) );
    return vec3( x, y, p_z );
}

// Pixel span of a 1 m object at camera z = −1 (paper S₀).
float saoProjScale( mat4 p_proj, float p_screenHeight )
{
    return 0.5 * p_screenHeight * abs( p_proj[ 1 ][ 1 ] );
}

// AlchemyAO XOR rotation hash (paper eq. 8).
float saoHashAngle( ivec2 p_pixel )
{
    const int x = p_pixel.x;
    const int y = p_pixel.y;
    return float( ( 30 * x ) ^ y + 10 * x * y );
}

// Pack AO ∈ [0,1] and camera Z into RGB8 (paper §2.3).
vec3 saoPackAOZ( float p_ao, float p_z )
{
    const float x = 256.0 * clamp( p_z / k_saoZmin, 0.0, 1.0 );
    return vec3( clamp( p_ao, 0.0, 1.0 ), floor( x ) / 256.0, fract( x ) );
}

float saoUnpackZ( vec3 p_packed )
{
    return k_saoZmin * ( p_packed.g * 256.0 + p_packed.b ) / 256.0;
}

// Recommended falloff kernel B from the SAO talk supplement.
float saoFalloffB( float p_vv, float p_vn, float p_radius2, float p_bias )
{
    const float f = max( p_radius2 - p_vv, 0.0 );
    return f * f * f * max( ( p_vn - p_bias ) / ( k_saoEpsilon + p_vv ), 0.0 );
}
