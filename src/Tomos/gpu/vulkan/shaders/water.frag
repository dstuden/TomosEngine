#version 450

// Scrolled normals, fresnel tint, clustered lights + shadows.
// Material: baseFactor=tint, roughFactor=specular softness, tNormal=waves.

layout( set = 0, binding = 0 ) uniform SceneUBO
{
#include "common/scene_ubo.glsl"
} scene;

#include "common/light.glsl"

layout( set = 0, binding = 2 ) readonly buffer LightSSBO
{
    Light lights[];
};

layout( set = 0, binding = 3 ) readonly buffer LightGrid
{
    LightCell cells[];
};

layout( set = 0, binding = 4 ) readonly buffer LightIndices
{
    uint indices[];
};

layout( set = 0, binding = 5 ) uniform sampler2DArrayShadow tShadow;

#include "common/point_shadow.glsl"

layout( set = 1, binding = 0 ) uniform MaterialUBO
{
    vec4  baseFactor;
    vec3  emissionFactor;
    float metalFactor;
    float roughFactor;
    float normalScale;
    float alphaCutoff;
    uint  ignoreAlpha;
    uint  hasNormalMap;
    uint  _pad0;
    uint  _pad1;
} material;

layout( set = 1, binding = 1 ) uniform sampler2D tBase;
layout( set = 1, binding = 2 ) uniform sampler2D tMetRgh;
layout( set = 1, binding = 3 ) uniform sampler2D tEmission;
layout( set = 1, binding = 4 ) uniform sampler2D tNormal;

layout( location = 0 ) in vec2 vUV;
layout( location = 1 ) in vec3 vWorldPos;
layout( location = 2 ) in vec3 vNormal;
layout( location = 3 ) in float vViewZ;
layout( location = 4 ) in vec4 vTangent;

layout( location = 0 ) out vec4 outColor;

const float PI             = 3.14159265359;
const float k_dielectricF0 = 0.04;
const float k_ambientScale = 0.04;

float distributionGGX( float p_ndoth, float p_rough )
{
    float a  = p_rough * p_rough;
    float a2 = a * a;
    float d  = p_ndoth * p_ndoth * ( a2 - 1.0 ) + 1.0;
    return a2 / max( PI * d * d, 1e-6 );
}

float geometrySmith( float p_ndotv, float p_ndotl, float p_rough )
{
    float r  = p_rough + 1.0;
    float k  = r * r / 8.0;
    float gv = p_ndotv / ( p_ndotv * ( 1.0 - k ) + k );
    float gl = p_ndotl / ( p_ndotl * ( 1.0 - k ) + k );
    return gv * gl;
}

vec3 fresnelSchlick( float p_vdoth, vec3 p_f0 )
{
    return p_f0 + ( 1.0 - p_f0 ) * pow( 1.0 - p_vdoth, 5.0 );
}

float shadowFactor( Light p_light, vec3 p_worldPos, float p_ndotl )
{
    if ( p_light.shadowMap < 0 ) return 1.0;

    const float bias  = clamp( 0.002 * tan( acos( clamp( p_ndotl, 0.0, 1.0 ) ) ), 0.0005, 0.01 );
    const vec2  texel = 1.0 / vec2( textureSize( tShadow, 0 ).xy );

    vec2  uv;
    float refDepth;
    float layer;

    if ( p_light.type == 0u )
    {
        const vec3 toFrag = p_worldPos - p_light.position;
        const int  face   = pointShadowFaceIndex( toFrag );
        const mat4 vp     = pointShadowFaceVP( p_light.position, p_light.maxRange, face );
        vec4       lp     = vp * vec4( p_worldPos, 1.0 );
        lp.xyz /= lp.w;

        uv       = lp.xy * 0.5 + 0.5;
        refDepth = lp.z;
        layer    = float( p_light.shadowMap + face );
    }
    else
    {
        vec4 lp = p_light.vp * vec4( p_worldPos, 1.0 );
        lp.xyz /= lp.w;

        uv       = lp.xy * 0.5 + 0.5;
        refDepth = lp.z;
        layer    = float( p_light.shadowMap );
    }

    if ( any( lessThan( uv, vec2( 0.0 ) ) ) || any( greaterThan( uv, vec2( 1.0 ) ) ) || refDepth > 1.0 )
        return 1.0;

    float sum = 0.0;
    for ( int x = -1; x <= 1; ++x )
        for ( int y = -1; y <= 1; ++y )
            sum += texture( tShadow, vec4( uv + vec2( x, y ) * texel, layer, refDepth - bias ) );
    return sum / 9.0;
}

vec3 shadeLight( Light p_light, vec3 p_n, vec3 p_v, vec3 p_albedo, float p_rough )
{
    vec3  l;
    float atten = 1.0;

    if ( p_light.type == 1u )
    {
        l = normalize( -p_light.direction );
    }
    else
    {
        const vec3  toLight = p_light.position - vWorldPos;
        const float dist    = length( toLight );
        l                   = toLight / max( dist, 1e-4 );

        const float range = max( p_light.maxRange, 1e-3 );
        atten             = clamp( 1.0 - dist / range, 0.0, 1.0 );
        atten *= atten;

        if ( p_light.type == 2u )
        {
            const float cosTheta = dot( -l, normalize( p_light.direction ) );
            atten *= smoothstep( p_light.outerCone, p_light.innerCone, cosTheta );
        }
    }

    const float ndotl = max( dot( p_n, l ), 0.0 );
    if ( ndotl <= 0.0 || atten <= 0.0 ) return vec3( 0.0 );

    const vec3  h     = normalize( l + p_v );
    const float ndotv = max( dot( p_n, p_v ), 1e-4 );
    const float ndoth = max( dot( p_n, h ), 0.0 );
    const float vdoth = max( dot( p_v, h ), 0.0 );

    const vec3 f0 = vec3( k_dielectricF0 );
    const vec3 f  = fresnelSchlick( vdoth, f0 );

    const float d = distributionGGX( ndoth, p_rough );
    const float g = geometrySmith( ndotv, ndotl, p_rough );

    const vec3 specular = ( d * g * f ) / max( 4.0 * ndotv * ndotl, 1e-4 );
    const vec3 diffuse  = ( 1.0 - f ) * p_albedo / PI;

    const float shadow = shadowFactor( p_light, vWorldPos, ndotl );

    return ( diffuse + specular ) * p_light.color * p_light.intensity * ndotl * atten * shadow;
}

void main()
{
    const float t   = scene.time;
    const vec2  uv1 = vUV * 2.5 + vec2( t * 0.03, t * 0.02 );
    const vec2  uv2 = vUV * 1.7 - vec2( t * 0.025, t * 0.018 );

    vec3 n = normalize( vNormal );
    if ( material.hasNormalMap != 0u )
    {
        const vec3 tgn = normalize( vTangent.xyz );
        const vec3 bit = cross( n, tgn ) * vTangent.w;
        const mat3 tbn = mat3( tgn, bit, n );

        vec3 n1 = texture( tNormal, uv1 ).xyz * 2.0 - 1.0;
        vec3 n2 = texture( tNormal, uv2 ).xyz * 2.0 - 1.0;
        n1.xy *= material.normalScale;
        n2.xy *= material.normalScale;
        n = normalize( tbn * normalize( n1 + n2 ) );
    }
    else
    {
        // Procedural ripples when no normal map is bound.
        const float w = 0.15 * cos( vWorldPos.x * 3.0 + t * 1.5 ) + 0.12 * cos( vWorldPos.z * 2.5 - t * 1.2 );
        n             = normalize( n + vec3( w, 0.0, w * 0.7 ) );
    }

    const vec3 v = normalize( scene.cameraPosNear.xyz - vWorldPos );
    if ( dot( n, v ) < 0.0 ) n = -n;

    const float fresnel = pow( 1.0 - clamp( dot( n, v ), 0.0, 1.0 ), 5.0 );

    const vec3 deep    = material.baseFactor.rgb * 0.35;
    const vec3 shallow = material.baseFactor.rgb;
    const vec3 albedo  = mix( deep, shallow, fresnel );

    const float rough = clamp( material.roughFactor, 0.04, 1.0 );

    const uvec3 grid = scene.clusterGrid.xyz;
    const float near = scene.cameraPosNear.w;
    const float far  = scene.screenFar.z;

    const uint cx = min( uint( gl_FragCoord.x / scene.screenFar.x * float( grid.x ) ), grid.x - 1u );
    const uint cy = min( uint( gl_FragCoord.y / scene.screenFar.y * float( grid.y ) ), grid.y - 1u );
    const uint cz = clamp( uint( log( max( vViewZ, near ) / near ) / log( far / near ) * float( grid.z ) ), 0u, grid.z - 1u );

    const LightCell cell = cells[ cx + cy * grid.x + cz * grid.x * grid.y ];

    vec3 color = albedo * k_ambientScale;
    color += material.emissionFactor * texture( tEmission, vUV ).rgb;

    for ( uint i = 0u; i < cell.count; ++i )
        color += shadeLight( lights[ indices[ cell.offset + i ] ], n, v, albedo, rough );

    // Brighten glancing angles (fake specular sky).
    color += fresnel * shallow * 0.35;

    const float alpha = mix( 0.45, 0.88, fresnel ) * material.baseFactor.a;
    outColor          = vec4( color, alpha );
}
