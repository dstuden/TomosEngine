#version 450

// Texture × tint for additive particles.
// Slot 0 (default white) also applies a soft disc so nullptr textures still glow.
// Color is NOT premultiplied — pipeline blends with SRC_ALPHA / ONE.

layout( set = 1, binding = 0 ) uniform sampler2D tParticle;

layout( push_constant ) uniform Push
{
    uint texIndex;
} pc;

layout( location = 0 ) in vec2  vUV;
layout( location = 1 ) in vec2  vLocalUV;
layout( location = 2 ) in vec4  vColor;
layout( location = 3 ) flat in uint vTexIndex;

layout( location = 0 ) out vec4 outColor;

const float k_alphaDiscard = 0.004;

void main()
{
    // One draw per texture batch — skip particles that belong to another slot.
    if ( vTexIndex != pc.texIndex ) discard;

    const vec4 texel = texture( tParticle, vUV );

    float a = texel.a * vColor.a;

    // Procedural soft edge only for the default (no-texture) slot.
    if ( vTexIndex == 0u )
    {
        float soft = length( vLocalUV - vec2( 0.5 ) ) * 2.0;
        soft       = clamp( 1.0 - soft, 0.0, 1.0 );
        soft       = soft * soft;  // quadratic falloff
        a *= soft;
    }

    if ( a < k_alphaDiscard ) discard;

    // Non-premultiplied RGB: blend (SRC_ALPHA, ONE) → dst + src.rgb * src.a.
    outColor = vec4( texel.rgb * vColor.rgb, a );
}
