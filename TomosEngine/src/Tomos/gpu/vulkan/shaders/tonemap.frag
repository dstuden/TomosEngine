#version 450

layout( set = 0, binding = 0 ) uniform sampler2D tHdr;

layout( location = 0 ) in vec2 vUV;
layout( location = 0 ) out vec4 outColor;

void main()
{
    vec3 color = texture( tHdr, vUV ).rgb;

    // Reinhard — swapchain is sRGB so the display does the gamma encode.
    color = color / ( color + vec3( 1.0 ) );

    outColor = vec4( color, 1.0 );
}
