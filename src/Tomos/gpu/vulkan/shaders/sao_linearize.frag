#version 450

#include "common/sao.glsl"

layout( set = 0, binding = 0 ) uniform sampler2D tDepth;

layout( push_constant ) uniform Push
{
    float nearPlane;
    float farPlane;
    float _pad0;
    float _pad1;
} pc;

layout( location = 0 ) in vec2 vUV;
layout( location = 0 ) out float outZ;

void main()
{
    const float d = texture( tDepth, vUV ).r;
    if ( d >= k_saoSkyDepth )
    {
        outZ = k_saoZmin;  // sentinel far
        return;
    }
    outZ = saoLinearZ( d, pc.nearPlane, pc.farPlane );
}
