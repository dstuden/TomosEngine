#version 450

#include "common/sao.glsl"

layout( set = 0, binding = 0 ) uniform sampler2D tLinearZ;

layout( push_constant ) uniform Push
{
    mat4  proj;
    vec4  screenSizeRadius;  // xy = size, z = world radius, w unused
    vec4  params;            // x = bias, y = sampleCount, z unused, w unused
} pc;

layout( location = 0 ) in vec2 vUV;
layout( location = 0 ) out float outAO;

// Interleaved gradient noise — less grid-like than XOR hash.
float saoInterleavedNoise( ivec2 p_pixel )
{
    return fract( 52.9829189 * fract( dot( vec2( p_pixel ), vec2( 0.06711056, 0.00583715 ) ) ) );
}

void main()
{
    const vec2  screenSize  = pc.screenSizeRadius.xy;
    const float radius      = max( pc.screenSizeRadius.z, 1e-4 );
    const float bias        = pc.params.x;
    const int   sampleCount = max( int( pc.params.y + 0.5 ), 1 );

    const ivec2 pixel = ivec2( gl_FragCoord.xy );
    const float zC    = texelFetch( tLinearZ, pixel, 0 ).r;

    if ( zC <= k_saoZmin + 1e-3 )
    {
        outAO = 1.0;
        return;
    }

    const vec3 C = saoReconstructCS( vec2( pixel ), zC, pc.proj, screenSize );
    const vec3 nC = normalize( cross( dFdy( C ), dFdx( C ) ) );

    const float projScale = saoProjScale( pc.proj, screenSize.y );
    const float radiusSS  = max( -radius * projScale / zC, 1.0 );
    const float radius2   = radius * radius;
    const float phi       = saoInterleavedNoise( pixel ) * 6.28318530718;

    float sum = 0.0;
    for ( int i = 0; i < sampleCount; ++i )
    {
        const float alpha = ( float( i ) + 0.5 ) / float( sampleCount );
        const float h     = radiusSS * alpha;
        const float theta = 6.28318530718 * alpha * k_saoTurns + phi;
        const vec2  dir   = vec2( cos( theta ), sin( theta ) );

        const ivec2 samplePx = pixel + ivec2( round( dir * h ) );
        if ( samplePx.x < 0 || samplePx.y < 0 || samplePx.x >= int( screenSize.x ) || samplePx.y >= int( screenSize.y ) )
            continue;

        const float zQ = texelFetch( tLinearZ, samplePx, 0 ).r;
        if ( zQ <= k_saoZmin + 1e-3 ) continue;

        const vec3  Q  = saoReconstructCS( vec2( samplePx ), zQ, pc.proj, screenSize );
        const vec3  v  = Q - C;
        const float vv = dot( v, v );
        const float vn = dot( v, nC );

        sum += saoFalloffB( vv, vn, radius2, bias );
    }

    const float norm = ( 2.0 * k_saoSigma ) / ( pow( radius, 6.0 ) * float( sampleCount ) );
    float       ao   = clamp( max( 0.0, 1.0 - sum * norm ), 0.0, 1.0 );

    // 2×2 micro bilateral via derivatives (paper §2.3).
    const float dzThresh = max( 0.05 * abs( zC ), 0.02 );
    float       w        = 1.0;
    float       accum    = ao;
    const float aoDx     = dFdx( ao );
    const float aoDy     = dFdy( ao );
    const float zDx      = dFdx( zC );
    const float zDy      = dFdy( zC );
    if ( abs( zDx ) < dzThresh )
    {
        accum += ao - aoDx;
        w += 1.0;
    }
    if ( abs( zDy ) < dzThresh )
    {
        accum += ao - aoDy;
        w += 1.0;
    }
    outAO = accum / w;
}
