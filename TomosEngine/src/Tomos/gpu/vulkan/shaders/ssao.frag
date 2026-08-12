#version 450

// Depth-difference AO with a world-space radius projected to UV at the
// fragment's depth plane (avoids distance-dependent dark banding).

layout( set = 0, binding = 0 ) uniform sampler2D tDepth;

layout( push_constant ) uniform Push
{
    mat4 projInv;
    vec4 params;  // x = world radius, y = bias, z = intensity, w reserved
} pc;

layout( location = 0 ) in vec2 vUV;
layout( location = 0 ) out vec4 outColor;

const float k_skyDepth     = 0.99999;
const float k_uvDelta      = 1e-3;
const float k_maxRadiusUV  = 0.08;
const float k_rangeHalf    = 0.5;

vec3 viewPos( float p_rawDepth, vec2 p_uv )
{
    // Vulkan NDC: xy in [-1,1], depth in [0,1].  Y flip is baked into proj.
    const vec4 clip = vec4( p_uv * 2.0 - 1.0, p_rawDepth, 1.0 );
    const vec4 view = pc.projInv * clip;
    return view.xyz / view.w;
}

void main()
{
    const float raw = texture( tDepth, vUV ).r;
    if ( raw >= k_skyDepth )
    {
        outColor = vec4( 1.0 );
        return;
    }

    const vec3  origin  = viewPos( raw, vUV );
    const float centerZ = abs( origin.z );

    const float radius    = max( pc.params.x, 1e-4 );
    const float bias      = pc.params.y;
    const float intensity = pc.params.z;

    // Map world radius → UV using the constant-depth plane at this fragment.
    const vec3  originDu    = viewPos( raw, vUV + vec2( k_uvDelta, 0.0 ) );
    const float metersPerUV = length( originDu.xy - origin.xy ) / k_uvDelta;
    const float radiusUV    = clamp( radius / max( metersPerUV, 1e-4 ), 0.0, k_maxRadiusUV );

    const vec2 offsets[ 8 ] = vec2[](
        vec2(  1.0,  0.0 ), vec2( -1.0,  0.0 ),
        vec2(  0.0,  1.0 ), vec2(  0.0, -1.0 ),
        vec2(  0.707,  0.707 ), vec2( -0.707,  0.707 ),
        vec2(  0.707, -0.707 ), vec2( -0.707, -0.707 ) );

    float occ       = 0.0;
    float weightSum = 0.0;
    for ( int i = 0; i < 8; ++i )
    {
        const vec2 suv = vUV + offsets[ i ] * radiusUV;
        if ( any( lessThan( suv, vec2( 0.0 ) ) ) || any( greaterThan( suv, vec2( 1.0 ) ) ) ) continue;

        const float sraw = texture( tDepth, suv ).r;
        if ( sraw >= k_skyDepth ) continue;

        const vec3  samplePos = viewPos( sraw, suv );
        const float dist      = length( samplePos - origin );
        // Ignore samples outside the AO sphere (large depth edges / silhouettes).
        const float rangeCheck = 1.0 - smoothstep( radius * k_rangeHalf, radius, dist );

        const float depthDiff = centerZ - abs( samplePos.z );
        if ( depthDiff > bias ) occ += rangeCheck;

        weightSum += 1.0;
    }

    float ao = 1.0;
    if ( weightSum > 0.0 ) ao = 1.0 - ( occ / weightSum ) * intensity;

    outColor = vec4( vec3( clamp( ao, 0.0, 1.0 ) ), 1.0 );
}
