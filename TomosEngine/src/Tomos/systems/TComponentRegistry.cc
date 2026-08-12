#include "Tomos/systems/TComponentRegistry.hh"

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "Tomos/gpu/vulkan/TVkGpu.hh"
#include "Tomos/systems/animation/TAnimatorComponent.hh"
#include "Tomos/systems/audio/TAudioComponent.hh"
#include "Tomos/systems/camera/TCameraComponent.hh"
#include "Tomos/systems/light/TLightComponent.hh"
#include "Tomos/systems/mesh/TMeshComponent.hh"
#include "Tomos/systems/particle/TParticleEmitterComponent.hh"
#include "Tomos/systems/physics/TColliderComponent.hh"
#include "Tomos/systems/physics/TRigidBodyComponent.hh"
#include "Tomos/systems/sprite/TSpriteComponent.hh"
#include "Tomos/systems/script/TScriptComponent.hh"
#include "Tomos/util/logger/TLogger.hh"

namespace Tomos
{
    using json = nlohmann::json;

    namespace
    {
        json vec2ToJson( const glm::vec2& v ) { return json::array( { v.x, v.y } ); }
        json vec3ToJson( const glm::vec3& v ) { return json::array( { v.x, v.y, v.z } ); }
        json vec4ToJson( const glm::vec4& v ) { return json::array( { v.x, v.y, v.z, v.w } ); }

        glm::vec2 jsonToVec2( const json& j, const glm::vec2& fallback = {} )
        {
            if ( !j.is_array() || j.size() < 2 ) return fallback;
            return { j[ 0 ].get<float>(), j[ 1 ].get<float>() };
        }

        glm::vec3 jsonToVec3( const json& j, const glm::vec3& fallback = {} )
        {
            if ( !j.is_array() || j.size() < 3 ) return fallback;
            return { j[ 0 ].get<float>(), j[ 1 ].get<float>(), j[ 2 ].get<float>() };
        }

        glm::vec4 jsonToVec4( const json& j, const glm::vec4& fallback = {} )
        {
            if ( !j.is_array() || j.size() < 4 ) return fallback;
            return { j[ 0 ].get<float>(), j[ 1 ].get<float>(), j[ 2 ].get<float>(), j[ 3 ].get<float>() };
        }

        const char* lightTypeName( TLightType t )
        {
            switch ( t )
            {
                case TLightType::Directional:
                    return "directional";
                case TLightType::Spot:
                    return "spot";
                default:
                    return "point";
            }
        }

        TLightType lightTypeFromName( const std::string& s )
        {
            if ( s == "directional" ) return TLightType::Directional;
            if ( s == "spot" ) return TLightType::Spot;
            return TLightType::Point;
        }

        const char* billboardName( TBillboardMode m )
        {
            switch ( m )
            {
                case TBillboardMode::Cylindrical:
                    return "cylindrical";
                case TBillboardMode::Fixed:
                    return "fixed";
                default:
                    return "spherical";
            }
        }

        TBillboardMode billboardFromName( const std::string& s )
        {
            if ( s == "cylindrical" ) return TBillboardMode::Cylindrical;
            if ( s == "fixed" ) return TBillboardMode::Fixed;
            return TBillboardMode::Spherical;
        }

        const char* colliderShapeName( TColliderShape s )
        {
            switch ( s )
            {
                case TColliderShape::Sphere:
                    return "sphere";
                case TColliderShape::Box:
                    return "box";
            }
            return "box";
        }

        TColliderShape colliderShapeFromName( const std::string& s )
        {
            if ( s == "sphere" ) return TColliderShape::Sphere;
            // "aabb" kept for older scene JSON.
            return TColliderShape::Box;
        }

        const TVkImage* resolveTexture( const json& j, TComponentResolveCtx& ctx )
        {
            if ( !j.contains( "texturePath" ) || !j[ "texturePath" ].is_string() ) return nullptr;
            const std::string path = j[ "texturePath" ].get<std::string>();
            if ( path.empty() || ctx.m_gpu == nullptr ) return nullptr;
            return ctx.m_bag.loadImage( *ctx.m_gpu, path );
        }

        json texturePathJson( const TVkImage* tex, const TComponentResolveCtx& ctx )
        {
            if ( tex == nullptr ) return nullptr;
            if ( const auto* path = ctx.m_bag.findImagePath( tex ) ) return *path;
            return nullptr;
        }
    }  // namespace

    TComponentRegistry& TComponentRegistry::get()
    {
        static TComponentRegistry s_instance;
        s_instance.ensureDefaults();
        return s_instance;
    }

    void TComponentRegistry::registerType( TComponentTypeInfo p_info )
    {
        const std::string type = p_info.m_type;
        const auto        it   = m_byType.find( type );
        if ( it != m_byType.end() )
        {
            m_types[ it->second ] = std::move( p_info );
            return;
        }
        m_byType[ type ] = m_types.size();
        m_types.push_back( std::move( p_info ) );
    }

    const TComponentTypeInfo* TComponentRegistry::find( const std::string& p_type ) const
    {
        const auto it = m_byType.find( p_type );
        return it != m_byType.end() ? &m_types[ it->second ] : nullptr;
    }

    std::string TComponentRegistry::typeOf( const TComponent& p_component ) const
    {
        // Skinned must be checked before mesh (inheritance).
        for ( const auto& info : m_types )
        {
            if ( info.m_matches( p_component ) ) return info.m_type;
        }
        return {};
    }

    nlohmann::json TComponentRegistry::saveComponent( const TComponent& p_component, const TComponentResolveCtx& p_ctx ) const
    {
        for ( const auto& info : m_types )
        {
            if ( !info.m_matches( p_component ) ) continue;
            json j = info.m_save( p_component, p_ctx );
            if ( j.is_null() ) return nullptr;
            j[ "type" ] = info.m_type;
            return j;
        }
        return nullptr;
    }

    std::shared_ptr<TComponent> TComponentRegistry::loadComponent( const nlohmann::json& p_json, TComponentResolveCtx& p_ctx ) const
    {
        if ( !p_json.is_object() || !p_json.contains( "type" ) ) return nullptr;
        const auto* info = find( p_json[ "type" ].get<std::string>() );
        if ( info == nullptr || !info->m_load ) return nullptr;
        return info->m_load( p_json, p_ctx );
    }

    std::shared_ptr<TComponent> TComponentRegistry::createDefault( const std::string& p_type ) const
    {
        const auto* info = find( p_type );
        if ( info == nullptr || !info->m_create ) return nullptr;
        return info->m_create();
    }

    void TComponentRegistry::wireSkinnedJoints( std::vector<TPendingSkinnedJoints>&                              p_pending,
                                                const std::unordered_map<uint64_t, std::shared_ptr<TSceneNode>>& p_byId )
    {
        for ( auto& entry : p_pending )
        {
            if ( entry.m_component == nullptr || !entry.m_joints.is_array() ) continue;
            entry.m_component->m_joints.clear();
            for ( const auto& jj : entry.m_joints )
            {
                TSkinJoint joint{};
                if ( jj.contains( "nodeId" ) )
                {
                    const uint64_t id = jj[ "nodeId" ].get<uint64_t>();
                    const auto     it = p_byId.find( id );
                    if ( it != p_byId.end() ) joint.m_node = it->second;
                }
                if ( jj.contains( "invBind" ) && jj[ "invBind" ].is_array() && jj[ "invBind" ].size() >= 16 )
                {
                    float m[ 16 ];
                    for ( int i = 0; i < 16; ++i ) m[ i ] = jj[ "invBind" ][ i ].get<float>();
                    joint.m_inverseBindMtx = glm::make_mat4( m );
                }
                entry.m_component->m_joints.push_back( std::move( joint ) );
            }
            entry.m_component->m_boneMatrices.assign( entry.m_component->m_joints.size(), glm::mat4( 1.0f ) );
        }
    }

    void TComponentRegistry::ensureDefaults()
    {
        if ( m_defaultsRegistered ) return;
        m_defaultsRegistered = true;

        registerType( TComponentTypeInfo{
                .m_type    = "camera",
                .m_label   = "Camera",
                .m_create  = [] { return std::make_shared<TCameraComponent>(); },
                .m_matches = []( const TComponent& c ) { return dynamic_cast<const TCameraComponent*>( &c ) != nullptr; },
                .m_save =
                        []( const TComponent& c, const TComponentResolveCtx& )
                {
                    const auto& cam = dynamic_cast<const TCameraComponent&>( c );
                    return json{
                            { "active", cam.m_active }, { "projection", cam.m_projection == TProjection::Orthographic ? "orthographic" : "perspective" },
                            { "fov", cam.m_fov },       { "near", cam.m_near },
                            { "far", cam.m_far },       { "orthoHalfHeight", cam.m_orthoHalfHeight },
                    };
                },
                .m_load =
                        []( const json& j, TComponentResolveCtx& )
                {
                    auto cam      = std::make_shared<TCameraComponent>();
                    cam->m_active = j.value( "active", true );
                    cam->m_projection =
                            j.value( "projection", std::string( "perspective" ) ) == "orthographic" ? TProjection::Orthographic : TProjection::Perspective;
                    cam->m_fov             = j.value( "fov", cam->m_fov );
                    cam->m_near            = j.value( "near", cam->m_near );
                    cam->m_far             = j.value( "far", cam->m_far );
                    cam->m_orthoHalfHeight = j.value( "orthoHalfHeight", cam->m_orthoHalfHeight );
                    cam->setDirty();
                    return cam;
                },
        } );

        registerType( TComponentTypeInfo{
                .m_type    = "light",
                .m_label   = "Light",
                .m_create  = [] { return std::make_shared<TLightComponent>(); },
                .m_matches = []( const TComponent& c ) { return dynamic_cast<const TLightComponent*>( &c ) != nullptr; },
                .m_save =
                        []( const TComponent& c, const TComponentResolveCtx& )
                {
                    const auto& lit = dynamic_cast<const TLightComponent&>( c );
                    return json{
                            { "lightType", lightTypeName( lit.m_type ) },
                            { "color", vec3ToJson( lit.m_color ) },
                            { "intensity", lit.m_intensity },
                            { "maxRange", lit.m_maxRange },
                            { "innerCone", lit.m_innerCone },
                            { "outerCone", lit.m_outerCone },
                            { "castShadow", lit.m_castShadow },
                    };
                },
                .m_load =
                        []( const json& j, TComponentResolveCtx& )
                {
                    auto lit          = std::make_shared<TLightComponent>();
                    lit->m_type       = lightTypeFromName( j.value( "lightType", std::string( "point" ) ) );
                    lit->m_color      = jsonToVec3( j.value( "color", json::array() ), lit->m_color );
                    lit->m_intensity  = j.value( "intensity", lit->m_intensity );
                    lit->m_maxRange   = j.value( "maxRange", lit->m_maxRange );
                    lit->m_innerCone  = j.value( "innerCone", lit->m_innerCone );
                    lit->m_outerCone  = j.value( "outerCone", lit->m_outerCone );
                    lit->m_castShadow = j.value( "castShadow", lit->m_castShadow );
                    return lit;
                },
        } );

        registerType( TComponentTypeInfo{
                .m_type    = "mesh",
                .m_label   = "Mesh",
                .m_create  = [] { return std::make_shared<TMeshComponent>( nullptr, nullptr, true ); },
                .m_matches = []( const TComponent& c )
                { return dynamic_cast<const TMeshComponent*>( &c ) != nullptr && dynamic_cast<const TSkinnedMeshComponent*>( &c ) == nullptr; },
                .m_save =
                        []( const TComponent& c, const TComponentResolveCtx& ctx )
                {
                    const auto&   mesh = dynamic_cast<const TMeshComponent&>( c );
                    TMeshAssetRef ref  = mesh.m_ref;
                    json          j{ { "castShadow", mesh.m_castShadow } };
                    if ( ref.empty() ) ( void ) ctx.m_assets.resolveMesh( mesh.m_mesh, mesh.m_material, ref );
                    if ( !ref.empty() )
                    {
                        j[ "asset" ]       = ref.m_assetName;
                        j[ "meshIdx" ]     = ref.m_meshIdx;
                        j[ "materialIdx" ] = ref.m_materialIdx;
                    }
                    return j;
                },
                .m_load =
                        []( const json& j, TComponentResolveCtx& ctx )
                {
                    TMeshAssetRef      ref{ j.value( "asset", std::string{} ), j.value( "meshIdx", 0u ), j.value( "materialIdx", 0u ) };
                    const TVkMesh*     mesh     = nullptr;
                    const TVkMaterial* material = nullptr;
                    if ( !ref.empty() )
                    {
                        if ( !ctx.m_assets.tryResolve( ref, mesh, material ) )
                            TLOG_WARN() << "[TComponentRegistry] mesh asset not found: " << ref.m_assetName;
                    }
                    return std::make_shared<TMeshComponent>( ref, mesh, material, ctx.m_assets.generation(), j.value( "castShadow", true ) );
                },
        } );

        registerType( TComponentTypeInfo{
                .m_type    = "skinnedMesh",
                .m_label   = "Skinned Mesh",
                .m_create  = [] { return std::make_shared<TSkinnedMeshComponent>( nullptr, nullptr, std::vector<TSkinJoint>{} ); },
                .m_matches = []( const TComponent& c ) { return dynamic_cast<const TSkinnedMeshComponent*>( &c ) != nullptr; },
                .m_save =
                        []( const TComponent& c, const TComponentResolveCtx& ctx )
                {
                    const auto&   mesh = dynamic_cast<const TSkinnedMeshComponent&>( c );
                    TMeshAssetRef ref  = mesh.m_ref;
                    json          j{ { "castShadow", mesh.m_castShadow } };
                    if ( ref.empty() ) ( void ) ctx.m_assets.resolveMesh( mesh.m_mesh, mesh.m_material, ref );
                    if ( !ref.empty() )
                    {
                        j[ "asset" ]       = ref.m_assetName;
                        j[ "meshIdx" ]     = ref.m_meshIdx;
                        j[ "materialIdx" ] = ref.m_materialIdx;
                    }
                    json joints = json::array();
                    for ( const auto& joint : mesh.m_joints )
                    {
                        json entry;
                        if ( auto node = joint.m_node.lock() ) entry[ "nodeId" ] = node->m_id;
                        json         inv = json::array();
                        const float* m   = glm::value_ptr( joint.m_inverseBindMtx );
                        for ( int i = 0; i < 16; ++i ) inv.push_back( m[ i ] );
                        entry[ "invBind" ] = inv;
                        joints.push_back( std::move( entry ) );
                    }
                    j[ "joints" ] = std::move( joints );
                    return j;
                },
                .m_load =
                        []( const json& j, TComponentResolveCtx& ctx )
                {
                    TMeshAssetRef      ref{ j.value( "asset", std::string{} ), j.value( "meshIdx", 0u ), j.value( "materialIdx", 0u ) };
                    const TVkMesh*     mesh     = nullptr;
                    const TVkMaterial* material = nullptr;
                    if ( !ref.empty() ) ( void ) ctx.m_assets.tryResolve( ref, mesh, material );
                    auto skinned          = std::make_shared<TSkinnedMeshComponent>( ref, mesh, material, ctx.m_assets.generation(), std::vector<TSkinJoint>{} );
                    skinned->m_castShadow = j.value( "castShadow", true );
                    if ( ctx.m_pendingSkinned != nullptr && j.contains( "joints" ) )
                        ctx.m_pendingSkinned->push_back( TPendingSkinnedJoints{ skinned.get(), j[ "joints" ] } );
                    return skinned;
                },
        } );

        registerType( TComponentTypeInfo{
                .m_type    = "animator",
                .m_label   = "Animator",
                .m_create  = [] { return std::make_shared<TAnimatorComponent>(); },
                .m_matches = []( const TComponent& c ) { return dynamic_cast<const TAnimatorComponent*>( &c ) != nullptr; },
                .m_save =
                        []( const TComponent& c, const TComponentResolveCtx& ctx )
                {
                    const auto& anim = dynamic_cast<const TAnimatorComponent&>( c );
                    json j{ { "playing", anim.m_playing },
                            { "looping", anim.m_looping },
                            { "speed", anim.m_speed },
                            { "time", anim.m_time },
                            { "defaultFade", anim.m_defaultFadeDuration } };
                    TClipAssetRef ref = anim.m_clipRef;
                    if ( ref.empty() && anim.m_clip != nullptr ) ( void ) ctx.m_assets.resolveClip( anim.m_clip, ref );
                    if ( !ref.empty() )
                    {
                        j[ "asset" ] = ref.m_assetName;
                        j[ "clip" ]  = ref.m_clipName;
                    }
                    return j;
                },
                .m_load =
                        []( const json& j, TComponentResolveCtx& ctx )
                {
                    auto anim                   = std::make_shared<TAnimatorComponent>();
                    anim->m_playing             = j.value( "playing", true );
                    anim->m_looping             = j.value( "looping", true );
                    anim->m_speed               = j.value( "speed", 1.0f );
                    anim->m_time                = j.value( "time", 0.0f );
                    anim->m_defaultFadeDuration = j.value( "defaultFade", 0.25f );
                    TClipAssetRef ref{ j.value( "asset", std::string{} ), j.value( "clip", std::string{} ) };
                    if ( !ref.empty() )
                    {
                        anim->m_clipRef         = ref;
                        anim->m_clip            = ctx.m_assets.tryResolve( ref );
                        anim->m_boundGeneration = ctx.m_assets.generation();
                    }
                    return anim;
                },
        } );

        registerType( TComponentTypeInfo{
                .m_type    = "sprite",
                .m_label   = "Sprite",
                .m_create  = [] { return std::make_shared<TSpriteComponent>(); },
                .m_matches = []( const TComponent& c ) { return dynamic_cast<const TSpriteComponent*>( &c ) != nullptr; },
                .m_save =
                        []( const TComponent& c, const TComponentResolveCtx& ctx )
                {
                    const auto& spr = dynamic_cast<const TSpriteComponent&>( c );
                    json        j{ { "size", vec2ToJson( spr.m_size ) },
                                   { "color", vec4ToJson( spr.m_color ) },
                                   { "uvMin", vec2ToJson( spr.m_uvMin ) },
                                   { "uvMax", vec2ToJson( spr.m_uvMax ) },
                                   { "rotation", spr.m_rotation },
                                   { "mode", billboardName( spr.m_mode ) },
                                   { "visible", spr.m_visible } };
                    if ( !spr.m_textureRef.empty() )
                        j[ "texturePath" ] = spr.m_textureRef.m_path;
                    else
                    {
                        auto texPath = texturePathJson( spr.m_texture, ctx );
                        if ( !texPath.is_null() ) j[ "texturePath" ] = texPath;
                    }
                    return j;
                },
                .m_load =
                        []( const json& j, TComponentResolveCtx& ctx )
                {
                    auto spr        = std::make_shared<TSpriteComponent>( resolveTexture( j, ctx ) );
                    if ( j.contains( "texturePath" ) && j[ "texturePath" ].is_string() )
                        spr->m_textureRef = TBagTextureRef{ j[ "texturePath" ].get<std::string>() };
                    spr->m_size     = jsonToVec2( j.value( "size", json::array() ), spr->m_size );
                    spr->m_color    = jsonToVec4( j.value( "color", json::array() ), spr->m_color );
                    spr->m_uvMin    = jsonToVec2( j.value( "uvMin", json::array() ), spr->m_uvMin );
                    spr->m_uvMax    = jsonToVec2( j.value( "uvMax", json::array() ), spr->m_uvMax );
                    spr->m_rotation = j.value( "rotation", spr->m_rotation );
                    spr->m_mode     = billboardFromName( j.value( "mode", std::string( "spherical" ) ) );
                    spr->m_visible  = j.value( "visible", true );
                    return spr;
                },
        } );

        registerType( TComponentTypeInfo{
                .m_type    = "particleEmitter",
                .m_label   = "Particle Emitter",
                .m_create  = [] { return std::make_shared<TParticleEmitterComponent>(); },
                .m_matches = []( const TComponent& c ) { return dynamic_cast<const TParticleEmitterComponent*>( &c ) != nullptr; },
                .m_save =
                        []( const TComponent& c, const TComponentResolveCtx& ctx )
                {
                    const auto& p = dynamic_cast<const TParticleEmitterComponent&>( c );
                    json        j{ { "emitting", p.m_emitting },
                                   { "rate", p.m_rate },
                                   { "lifetimeMin", p.m_lifetimeMin },
                                   { "lifetimeMax", p.m_lifetimeMax },
                                   { "velocityMin", vec3ToJson( p.m_velocityMin ) },
                                   { "velocityMax", vec3ToJson( p.m_velocityMax ) },
                                   { "sizeStart", vec2ToJson( p.m_sizeStart ) },
                                   { "sizeEnd", vec2ToJson( p.m_sizeEnd ) },
                                   { "colorStart", vec4ToJson( p.m_colorStart ) },
                                   { "colorEnd", vec4ToJson( p.m_colorEnd ) },
                                   { "gravity", p.m_gravity },
                                   { "uvMin", vec2ToJson( p.m_uvMin ) },
                                   { "uvMax", vec2ToJson( p.m_uvMax ) },
                                   { "seed", p.m_seed } };
                    auto        texPath = texturePathJson( p.m_texture, ctx );
                    if ( !texPath.is_null() ) j[ "texturePath" ] = texPath;
                    return j;
                },
                .m_load =
                        []( const json& j, TComponentResolveCtx& ctx )
                {
                    auto p           = std::make_shared<TParticleEmitterComponent>();
                    p->m_emitting    = j.value( "emitting", true );
                    p->m_rate        = j.value( "rate", p->m_rate );
                    p->m_lifetimeMin = j.value( "lifetimeMin", p->m_lifetimeMin );
                    p->m_lifetimeMax = j.value( "lifetimeMax", p->m_lifetimeMax );
                    p->m_velocityMin = jsonToVec3( j.value( "velocityMin", json::array() ), p->m_velocityMin );
                    p->m_velocityMax = jsonToVec3( j.value( "velocityMax", json::array() ), p->m_velocityMax );
                    p->m_sizeStart   = jsonToVec2( j.value( "sizeStart", json::array() ), p->m_sizeStart );
                    p->m_sizeEnd     = jsonToVec2( j.value( "sizeEnd", json::array() ), p->m_sizeEnd );
                    p->m_colorStart  = jsonToVec4( j.value( "colorStart", json::array() ), p->m_colorStart );
                    p->m_colorEnd    = jsonToVec4( j.value( "colorEnd", json::array() ), p->m_colorEnd );
                    p->m_gravity     = j.value( "gravity", p->m_gravity );
                    p->m_uvMin       = jsonToVec2( j.value( "uvMin", json::array() ), p->m_uvMin );
                    p->m_uvMax       = jsonToVec2( j.value( "uvMax", json::array() ), p->m_uvMax );
                    p->m_seed        = j.value( "seed", p->m_seed );
                    p->m_texture     = resolveTexture( j, ctx );
                    return p;
                },
        } );

        registerType( TComponentTypeInfo{
                .m_type    = "audio",
                .m_label   = "Audio",
                .m_create  = [] { return std::make_shared<TAudioComponent>(); },
                .m_matches = []( const TComponent& c ) { return dynamic_cast<const TAudioComponent*>( &c ) != nullptr; },
                .m_save =
                        []( const TComponent& c, const TComponentResolveCtx& ctx )
                {
                    const auto& a = dynamic_cast<const TAudioComponent&>( c );
                    json j{ { "volume", a.m_volume },   { "pitch", a.m_pitch },     { "minDistance", a.m_minDistance }, { "maxDistance", a.m_maxDistance },
                            { "looping", a.m_looping }, { "spatial", a.m_spatial }, { "playOnStart", a.m_playOnStart } };
                    if ( a.m_clip != nullptr )
                    {
                        if ( const auto* path = ctx.m_bag.findClipPath( a.m_clip ) )
                            j[ "clipPath" ] = *path;
                        else if ( !a.m_clip->m_path.empty() )
                            j[ "clipPath" ] = a.m_clip->m_path;
                        j[ "clipName" ] = a.m_clip->m_name;
                    }
                    return j;
                },
                .m_load =
                        []( const json& j, TComponentResolveCtx& ctx )
                {
                    const TAudioClip* clip = nullptr;
                    const std::string path = j.value( "clipPath", std::string{} );
                    if ( !path.empty() ) clip = ctx.m_bag.getOrCreateClip( path, j.value( "clipName", std::string{} ) );
                    auto a           = std::make_shared<TAudioComponent>( clip );
                    a->m_volume      = j.value( "volume", a->m_volume );
                    a->m_pitch       = j.value( "pitch", a->m_pitch );
                    a->m_minDistance = j.value( "minDistance", a->m_minDistance );
                    a->m_maxDistance = j.value( "maxDistance", a->m_maxDistance );
                    a->m_looping     = j.value( "looping", a->m_looping );
                    a->m_spatial     = j.value( "spatial", a->m_spatial );
                    a->m_playOnStart = j.value( "playOnStart", a->m_playOnStart );
                    return a;
                },
        } );

        registerType( TComponentTypeInfo{
                .m_type    = "rigidBody",
                .m_label   = "Rigid Body",
                .m_create  = [] { return std::make_shared<TRigidBodyComponent>(); },
                .m_matches = []( const TComponent& c ) { return dynamic_cast<const TRigidBodyComponent*>( &c ) != nullptr; },
                .m_save =
                        []( const TComponent& c, const TComponentResolveCtx& )
                {
                    const auto& b = dynamic_cast<const TRigidBodyComponent&>( c );
                    return json{ { "mass", b.m_mass },
                                 { "linearVelocity", vec3ToJson( b.m_linearVelocity ) },
                                 { "gravityScale", b.m_gravityScale },
                                 { "linearDamping", b.m_linearDamping },
                                 { "restitution", b.m_restitution },
                                 { "useGravity", b.m_useGravity },
                                 { "kinematic", b.m_kinematic } };
                },
                .m_load =
                        []( const json& j, TComponentResolveCtx& )
                {
                    auto b = std::make_shared<TRigidBodyComponent>();
                    b->setMass( j.value( "mass", b->m_mass ) );
                    b->m_linearVelocity = jsonToVec3( j.value( "linearVelocity", json::array() ), b->m_linearVelocity );
                    b->m_gravityScale   = j.value( "gravityScale", b->m_gravityScale );
                    b->m_linearDamping  = j.value( "linearDamping", b->m_linearDamping );
                    b->m_restitution    = j.value( "restitution", b->m_restitution );
                    b->m_useGravity     = j.value( "useGravity", b->m_useGravity );
                    b->m_kinematic      = j.value( "kinematic", b->m_kinematic );
                    return b;
                },
        } );

        registerType( TComponentTypeInfo{
                .m_type    = "collider",
                .m_label   = "Collider",
                .m_create  = [] { return std::make_shared<TColliderComponent>(); },
                .m_matches = []( const TComponent& c ) { return dynamic_cast<const TColliderComponent*>( &c ) != nullptr; },
                .m_save =
                        []( const TComponent& c, const TComponentResolveCtx& )
                {
                    const auto& col = dynamic_cast<const TColliderComponent&>( c );
                    return json{ { "shape", colliderShapeName( col.m_shape ) },
                                 { "radius", col.m_radius },
                                 { "halfExtents", vec3ToJson( col.m_halfExtents ) },
                                 { "isTrigger", col.m_isTrigger },
                                 { "enabled", col.m_enabled },
                                 { "layer", col.m_layer },
                                 { "mask", col.m_mask } };
                },
                .m_load =
                        []( const json& j, TComponentResolveCtx& )
                {
                    auto col           = std::make_shared<TColliderComponent>();
                    col->m_shape       = colliderShapeFromName( j.value( "shape", std::string( "aabb" ) ) );
                    col->m_radius      = j.value( "radius", col->m_radius );
                    col->m_halfExtents = jsonToVec3( j.value( "halfExtents", json::array() ), col->m_halfExtents );
                    col->m_isTrigger   = j.value( "isTrigger", col->m_isTrigger );
                    col->m_enabled     = j.value( "enabled", col->m_enabled );
                    col->m_layer       = j.value( "layer", col->m_layer );
                    col->m_mask        = j.value( "mask", col->m_mask );
                    return col;
                },
        } );

        registerType( TComponentTypeInfo{
                .m_type    = "script",
                .m_label   = "Script",
                .m_create  = nullptr,
                .m_matches = []( const TComponent& c ) { return dynamic_cast<const TScriptComponent*>( &c ) != nullptr; },
                .m_save =
                        []( const TComponent& c, const TComponentResolveCtx& )
                {
                    const auto& sc = dynamic_cast<const TScriptComponent&>( c );
                    const std::string scriptType = sc.typeName();
                    if ( scriptType.empty() ) return json( nullptr );
                    return json{ { "script", scriptType } };
                },
                .m_load =
                        []( const json& j, TComponentResolveCtx& )
                {
                    const std::string scriptType = j.value( "script", std::string{} );
                    if ( scriptType.empty() ) return std::shared_ptr<TComponent>{};
                    return std::static_pointer_cast<TComponent>( TScriptComponent::makeFromType( scriptType ) );
                },
        } );
    }
}  // namespace Tomos
