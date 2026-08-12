#version 450

layout( set = 0, binding = 0 ) uniform sampler2D tHdr;
layout( set = 0, binding = 1 ) uniform sampler2D tDepth;

layout( push_constant ) uniform Push
{
    mat4  projInv;
    vec4  fogColorDensity;  // rgb + density
} pc;

layout( location = 0 ) in vec2 vUV;
layout( location = 0 ) out vec4 outColor;

float viewDepth( float p_rawDepth, vec2 p_uv )
{
    // Vulkan NDC: xy in [-1,1], depth in [0,1].  Y flip is baked into proj.
    const vec4 clip = vec4( p_uv * 2.0 - 1.0, p_rawDepth, 1.0 );
    const vec4 view = pc.projInv * clip;
    return abs( view.z / view.w );
}

void main()
{
    const vec3  color = texture( tHdr, vUV ).rgb;
    const float depth = texture( tDepth, vUV ).r;
    const float viewZ = viewDepth( depth, vUV );

    const float fogFactor = 1.0 - exp( -pc.fogColorDensity.a * viewZ );
    const vec3  fogged    = mix( color, pc.fogColorDensity.rgb, clamp( fogFactor, 0.0, 1.0 ) );

    outColor = vec4( fogged, 1.0 );
}
