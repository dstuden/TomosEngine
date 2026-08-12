// Six vertices → two triangles for a unit quad centered at origin.
// Corners in [-0.5, 0.5]; add 0.5 for UV space.

const vec2 k_corners[ 6 ] = vec2[](
    vec2( -0.5, -0.5 ), vec2( 0.5, -0.5 ), vec2( -0.5, 0.5 ),
    vec2( -0.5,  0.5 ), vec2( 0.5, -0.5 ), vec2(  0.5, 0.5 ) );
