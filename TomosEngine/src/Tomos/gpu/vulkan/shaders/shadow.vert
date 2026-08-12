#version 450

// Depth-only shadow pass.  The light view-projection arrives via push
// constant; instance transforms come from the shared instance SSBO.

layout( push_constant ) uniform Push
{
    mat4 lightVP;
} pc;

#include "common/instance.glsl"

layout( set = 0, binding = 1 ) readonly buffer InstanceSSBO
{
    Instance instances[];
};

layout( location = 0 ) in vec3 inPosition;

void main()
{
    gl_Position = pc.lightVP * instances[ gl_InstanceIndex ].transform * vec4( inPosition, 1.0 );
}
