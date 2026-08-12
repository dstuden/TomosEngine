// Point-light cubemap faces packed into sampler2DArrayShadow layers.
// Face order / up vectors must match Tomos::pointShadowFaceBasis (TPointShadow.hh).

const float k_pointShadowNear = 0.1;
const float k_halfPi          = 1.57079632679;

mat4 pointShadowLookAt( vec3 p_eye, vec3 p_target, vec3 p_up )
{
    const vec3 f = normalize( p_target - p_eye );
    const vec3 s = normalize( cross( f, p_up ) );
    const vec3 u = cross( s, f );
    return mat4( vec4( s, 0.0 ), vec4( u, 0.0 ), vec4( -f, 0.0 ),
                 vec4( -dot( s, p_eye ), -dot( u, p_eye ), dot( f, p_eye ), 1.0 ) );
}

// GLM_FORCE_DEPTH_ZERO_TO_ONE + Vulkan Y flip (matches perspectiveVk).
mat4 pointShadowPerspective( float p_fovy, float p_near, float p_far )
{
    const float f = 1.0 / tan( p_fovy * 0.5 );
    mat4        m = mat4( 0.0 );
    m[ 0 ][ 0 ]   = f;
    m[ 1 ][ 1 ]   = -f;
    m[ 2 ][ 2 ]   = p_far / ( p_near - p_far );
    m[ 2 ][ 3 ]   = -1.0;
    m[ 3 ][ 2 ]   = -( p_far * p_near ) / ( p_far - p_near );
    return m;
}

void pointShadowFaceBasis( int p_face, out vec3 p_target, out vec3 p_up )
{
    if ( p_face == 0 )
    {
        p_target = vec3( 1.0, 0.0, 0.0 );
        p_up     = vec3( 0.0, -1.0, 0.0 );
    }
    else if ( p_face == 1 )
    {
        p_target = vec3( -1.0, 0.0, 0.0 );
        p_up     = vec3( 0.0, -1.0, 0.0 );
    }
    else if ( p_face == 2 )
    {
        p_target = vec3( 0.0, 1.0, 0.0 );
        p_up     = vec3( 0.0, 0.0, 1.0 );
    }
    else if ( p_face == 3 )
    {
        p_target = vec3( 0.0, -1.0, 0.0 );
        p_up     = vec3( 0.0, 0.0, -1.0 );
    }
    else if ( p_face == 4 )
    {
        p_target = vec3( 0.0, 0.0, 1.0 );
        p_up     = vec3( 0.0, -1.0, 0.0 );
    }
    else
    {
        p_target = vec3( 0.0, 0.0, -1.0 );
        p_up     = vec3( 0.0, -1.0, 0.0 );
    }
}

int pointShadowFaceIndex( vec3 p_dir )
{
    const vec3 a = abs( p_dir );
    if ( a.x >= a.y && a.x >= a.z ) return p_dir.x > 0.0 ? 0 : 1;
    if ( a.y >= a.z ) return p_dir.y > 0.0 ? 2 : 3;
    return p_dir.z > 0.0 ? 4 : 5;
}

mat4 pointShadowFaceVP( vec3 p_lightPos, float p_range, int p_face )
{
    vec3 target, up;
    pointShadowFaceBasis( p_face, target, up );
    const float farZ = max( p_range, k_pointShadowNear + 0.01 );
    return pointShadowPerspective( k_halfPi, k_pointShadowNear, farZ ) *
           pointShadowLookAt( p_lightPos, p_lightPos + target, up );
}
