#version 450

layout( set = 0, binding = 0 ) uniform sampler2D tCurrent;
layout( set = 0, binding = 1 ) uniform sampler2D tHistory;

layout( push_constant ) uniform Push
{
    float historyWeight;  // 0 = current only, higher = smoother / more lag
    float _pad0;
    float _pad1;
    float _pad2;
} pc;

layout( location = 0 ) in vec2 vUV;
layout( location = 0 ) out float outAO;

void main()
{
    const float cur  = texture( tCurrent, vUV ).r;
    const float hist = texture( tHistory, vUV ).r;
    outAO = mix( cur, hist, clamp( pc.historyWeight, 0.0, 0.95 ) );
}
