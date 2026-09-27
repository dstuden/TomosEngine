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
#include "Tomos/systems/script/TScriptComponent.hh"
#include "Tomos/systems/sprite/TSpriteComponent.hh"
#include "Tomos/util/logger/TLogger.hh"
#include "Tomos/util/reflect/TReflectComponent.hh"
#include "Tomos/util/reflect/TReflectEnum.hh"

namespace Tomos
{
    using json = nlohmann::json;

    namespace
    {
        json vec2ToJson( const glm::vec2& p_v ) { return json::array( { p_v.x, p_v.y } ); }
        json vec3ToJson( const glm::vec3& p_v ) { return json::array( { p_v.x, p_v.y, p_v.z } ); }
        json vec4ToJson( const glm::vec4& p_v ) { return json::array( { p_v.x, p_v.y, p_v.z, p_v.w } ); }

        glm::vec2 jsonToVec2( const json& p_j, const glm::vec2& p_fallback = {} )
        {
            if ( !p_j.is_array() || p_j.size() < 2 ) return p_fallback;
            return { p_j[ 0 ].get<float>(), p_j[ 1 ].get<float>() };
        }

        glm::vec3 jsonToVec3( const json& p_j, const glm::vec3& p_fallback = {} )
        {
            if ( !p_j.is_array() || p_j.size() < 3 ) return p_fallback;
            return { p_j[ 0 ].get<float>(), p_j[ 1 ].get<float>(), p_j[ 2 ].get<float>() };
        }

        glm::vec4 jsonToVec4( const json& p_j, const glm::vec4& p_fallback = {} )
        {
            if ( !p_j.is_array() || p_j.size() < 4 ) return p_fallback;
            return { p_j[ 0 ].get<float>(), p_j[ 1 ].get<float>(), p_j[ 2 ].get<float>(), p_j[ 3 ].get<float>() };
        }

        TBagAnimatedTextureRef animRefFromJson( const json& p_j, const std::string& p_pathKey = "texturePath" )
        {
            TBagAnimatedTextureRef ref{};
            if ( p_j.contains( p_pathKey ) && p_j[ p_pathKey ].is_string() ) ref.m_path = p_j[ p_pathKey ].get<std::string>();
            ref.m_looping = p_j.value( "animLoop", true );
            ref.m_playing = p_j.value( "animPlaying", true );
            ref.m_speed   = p_j.value( "animSpeed", 1.0f );
            return ref;
        }

        void writeAnimFields( json& p_j, const TBagAnimatedTextureRef& p_ref )
        {
            if ( p_ref.empty() ) return;
            if ( !p_ref.m_looping ) p_j[ "animLoop" ] = false;
            if ( !p_ref.m_playing ) p_j[ "animPlaying" ] = false;
            if ( p_ref.m_speed != 1.0f ) p_j[ "animSpeed" ] = p_ref.m_speed;
        }

        const TVkImage* resolveTexture( const json& p_j, TComponentResolveCtx& p_ctx, TBagAnimatedTextureRef* p_outRef = nullptr )
        {
            TBagAnimatedTextureRef ref = animRefFromJson( p_j );
            if ( p_outRef != nullptr ) *p_outRef = ref;
            if ( ref.empty() || p_ctx.m_gpu == nullptr ) return nullptr;
            return p_ctx.m_bag.resolveTexture( *p_ctx.m_gpu, ref.m_path, ref );
        }

        json texturePathJson( const TVkImage* p_tex, const TComponentResolveCtx& p_ctx )
        {
            if ( p_tex == nullptr ) return nullptr;
            if ( const auto* path = p_ctx.m_bag.findImagePath( p_tex ) ) return *path;
            return nullptr;
        }

        TBagAnimatedTextureRef overrideRefFromJson( const json& p_j, const char* p_pathKey, const char* p_loopKey, const char* p_playKey,
                                                    const char* p_speedKey )
        {
            TBagAnimatedTextureRef ref{};
            if ( p_j.contains( p_pathKey ) && p_j[ p_pathKey ].is_string() ) ref.m_path = p_j[ p_pathKey ].get<std::string>();
            ref.m_looping = p_j.value( p_loopKey, true );
            ref.m_playing = p_j.value( p_playKey, true );
            ref.m_speed   = p_j.value( p_speedKey, 1.0f );
            return ref;
        }
    }  // namespace

    TComponentRegistry& TComponentRegistry::get()
    {
        static TComponentRegistry sInstance;
        sInstance.ensureDefaults();
        return sInstance;
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

    std::unique_ptr<TComponent> TComponentRegistry::loadComponent( const nlohmann::json& p_json, TComponentResolveCtx& p_ctx ) const
    {
        if ( !p_json.is_object() || !p_json.contains( "type" ) ) return nullptr;
        const auto* info = find( p_json[ "type" ].get<std::string>() );
        if ( info == nullptr || !info->m_load ) return nullptr;
        return info->m_load( p_json, p_ctx );
    }

    std::unique_ptr<TComponent> TComponentRegistry::createDefault( const std::string& p_type ) const
    {
        const auto* info = find( p_type );
        if ( info == nullptr || !info->m_create ) return nullptr;
        return info->m_create();
    }

    void TComponentRegistry::wireSkinnedJoints( std::vector<TPendingSkinnedJoints>& p_pending, const std::unordered_map<uint64_t, TSceneNode*>& p_byId )
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
                    if ( it != p_byId.end() ) joint.bind( it->second );
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

        registerType( Reflect::makePodComponent<TCameraComponent>( []( TCameraComponent& p_c ) { p_c.setDirty(); } ) );

        registerType( Reflect::makePodComponent<TLightComponent>() );

        registerType( Reflect::makeComponentStub<TMeshComponent, TSkinnedMeshComponent>(
                "mesh", "Mesh",
                []( const TComponent& p_c, const TComponentResolveCtx& p_ctx )
                {
                    const auto&   mesh = dynamic_cast<const TMeshComponent&>( p_c );
                    TMeshAssetRef ref  = mesh.m_ref;
                    json          j{ { "castShadow", mesh.m_castShadow } };
                    if ( ref.empty() ) ( void ) p_ctx.m_assets.resolveMesh( mesh.m_mesh, mesh.m_material, ref );
                    if ( !ref.empty() )
                    {
                        j[ "asset" ]       = ref.m_assetName;
                        j[ "meshIdx" ]     = ref.m_meshIdx;
                        j[ "materialIdx" ] = ref.m_materialIdx;
                    }
                    if ( !mesh.m_baseTextureOverride.empty() )
                    {
                        j[ "baseTexturePath" ] = mesh.m_baseTextureOverride.m_path;
                        if ( !mesh.m_baseTextureOverride.m_looping ) j[ "baseAnimLoop" ] = false;
                        if ( !mesh.m_baseTextureOverride.m_playing ) j[ "baseAnimPlaying" ] = false;
                        if ( mesh.m_baseTextureOverride.m_speed != 1.0f ) j[ "baseAnimSpeed" ] = mesh.m_baseTextureOverride.m_speed;
                    }
                    if ( !mesh.m_emissionTextureOverride.empty() )
                    {
                        j[ "emissionTexturePath" ] = mesh.m_emissionTextureOverride.m_path;
                        if ( !mesh.m_emissionTextureOverride.m_looping ) j[ "emissionAnimLoop" ] = false;
                        if ( !mesh.m_emissionTextureOverride.m_playing ) j[ "emissionAnimPlaying" ] = false;
                        if ( mesh.m_emissionTextureOverride.m_speed != 1.0f ) j[ "emissionAnimSpeed" ] = mesh.m_emissionTextureOverride.m_speed;
                    }
                    return j;
                },
                []( const json& p_j, TComponentResolveCtx& p_ctx )
                {
                    TMeshAssetRef      ref{ p_j.value( "asset", std::string{} ), p_j.value( "meshIdx", 0u ), p_j.value( "materialIdx", 0u ) };
                    const TVkMesh*     mesh     = nullptr;
                    const TVkMaterial* material = nullptr;
                    if ( !ref.empty() )
                    {
                        if ( !p_ctx.m_assets.tryResolve( ref, mesh, material ) )
                            TLOG_WARN() << "[TComponentRegistry] mesh asset not found: " << ref.m_assetName;
                    }
                    auto mc = std::make_unique<TMeshComponent>( ref, mesh, material, p_ctx.m_assets.generation(), p_j.value( "castShadow", true ) );
                    mc->m_baseTextureOverride = overrideRefFromJson( p_j, "baseTexturePath", "baseAnimLoop", "baseAnimPlaying", "baseAnimSpeed" );
                    mc->m_emissionTextureOverride =
                            overrideRefFromJson( p_j, "emissionTexturePath", "emissionAnimLoop", "emissionAnimPlaying", "emissionAnimSpeed" );
                    if ( p_ctx.m_gpu != nullptr && ( !mc->m_baseTextureOverride.empty() || !mc->m_emissionTextureOverride.empty() ) )
                        mc->rebindOverrides( p_ctx.m_bag, *p_ctx.m_gpu );
                    return mc;
                },
                [] { return std::make_unique<TMeshComponent>( nullptr, nullptr, true ); } ) );

        registerType( Reflect::makeComponentStub<TSkinnedMeshComponent>(
                "skinnedMesh", "Skinned Mesh",
                []( const TComponent& p_c, const TComponentResolveCtx& p_ctx )
                {
                    const auto&   mesh = dynamic_cast<const TSkinnedMeshComponent&>( p_c );
                    TMeshAssetRef ref  = mesh.m_ref;
                    json          j{ { "castShadow", mesh.m_castShadow } };
                    if ( ref.empty() ) ( void ) p_ctx.m_assets.resolveMesh( mesh.m_mesh, mesh.m_material, ref );
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
                        if ( const TSceneNode* jointNode = joint.liveNode( p_ctx.m_store ) ) entry[ "nodeId" ] = jointNode->m_id;
                        json         inv = json::array();
                        const float* m   = glm::value_ptr( joint.m_inverseBindMtx );
                        for ( int i = 0; i < 16; ++i ) inv.push_back( m[ i ] );
                        entry[ "invBind" ] = inv;
                        joints.push_back( std::move( entry ) );
                    }
                    j[ "joints" ] = std::move( joints );
                    return j;
                },
                []( const json& p_j, TComponentResolveCtx& p_ctx )
                {
                    TMeshAssetRef      ref{ p_j.value( "asset", std::string{} ), p_j.value( "meshIdx", 0u ), p_j.value( "materialIdx", 0u ) };
                    const TVkMesh*     mesh     = nullptr;
                    const TVkMaterial* material = nullptr;
                    if ( !ref.empty() ) ( void ) p_ctx.m_assets.tryResolve( ref, mesh, material );
                    auto skinned = std::make_unique<TSkinnedMeshComponent>( ref, mesh, material, p_ctx.m_assets.generation(), std::vector<TSkinJoint>{} );
                    skinned->m_castShadow = p_j.value( "castShadow", true );
                    if ( p_ctx.m_pendingSkinned != nullptr && p_j.contains( "joints" ) )
                        p_ctx.m_pendingSkinned->push_back( TPendingSkinnedJoints{ skinned.get(), p_j[ "joints" ] } );
                    return skinned;
                },
                [] { return std::make_unique<TSkinnedMeshComponent>( nullptr, nullptr, std::vector<TSkinJoint>{} ); } ) );

        registerType( Reflect::makeComponentStub<TAnimatorComponent>(
                "animator", "Animator",
                []( const TComponent& p_c, const TComponentResolveCtx& p_ctx )
                {
                    const auto&   anim = dynamic_cast<const TAnimatorComponent&>( p_c );
                    json          j{ { "playing", anim.m_playing },
                                     { "looping", anim.m_looping },
                                     { "speed", anim.m_speed },
                                     { "time", anim.m_time },
                                     { "defaultFade", anim.m_defaultFadeDuration } };
                    TClipAssetRef ref = anim.m_clipRef;
                    if ( ref.empty() && anim.m_clip != nullptr ) ( void ) p_ctx.m_assets.resolveClip( anim.m_clip, ref );
                    if ( !ref.empty() )
                    {
                        j[ "asset" ] = ref.m_assetName;
                        j[ "clip" ]  = ref.m_clipName;
                    }
                    return j;
                },
                []( const json& p_j, TComponentResolveCtx& p_ctx )
                {
                    auto anim                   = std::make_unique<TAnimatorComponent>();
                    anim->m_playing             = p_j.value( "playing", true );
                    anim->m_looping             = p_j.value( "looping", true );
                    anim->m_speed               = p_j.value( "speed", 1.0f );
                    anim->m_time                = p_j.value( "time", 0.0f );
                    anim->m_defaultFadeDuration = p_j.value( "defaultFade", 0.25f );
                    TClipAssetRef ref{ p_j.value( "asset", std::string{} ), p_j.value( "clip", std::string{} ) };
                    if ( !ref.empty() )
                    {
                        anim->m_clipRef         = ref;
                        anim->m_clip            = p_ctx.m_assets.tryResolve( ref );
                        anim->m_boundGeneration = p_ctx.m_assets.generation();
                    }
                    return anim;
                } ) );

        registerType( Reflect::makeComponentStub<TSpriteComponent>(
                "sprite", "Sprite",
                []( const TComponent& p_c, const TComponentResolveCtx& p_ctx )
                {
                    const auto& spr = dynamic_cast<const TSpriteComponent&>( p_c );
                    json        j{ { "size", vec2ToJson( spr.m_size ) },
                                   { "color", vec4ToJson( spr.m_color ) },
                                   { "uvMin", vec2ToJson( spr.m_uvMin ) },
                                   { "uvMax", vec2ToJson( spr.m_uvMax ) },
                                   { "rotation", spr.m_rotation },
                                   { "mode", Reflect::enumNameLower( spr.m_mode ) },
                                   { "alphaMode", Reflect::enumNameLower( spr.m_alphaMode ) },
                                   { "alphaCutoff", spr.m_alphaCutoff },
                                   { "visible", spr.m_visible } };
                    if ( !spr.m_animRef.empty() )
                    {
                        j[ "texturePath" ] = spr.m_animRef.m_path;
                        writeAnimFields( j, spr.m_animRef );
                    }
                    else if ( !spr.m_textureRef.empty() )
                    {
                        j[ "texturePath" ] = spr.m_textureRef.m_path;
                    }
                    else
                    {
                        auto texPath = texturePathJson( spr.m_texture, p_ctx );
                        if ( !texPath.is_null() ) j[ "texturePath" ] = texPath;
                    }
                    return j;
                },
                []( const json& p_j, TComponentResolveCtx& p_ctx )
                {
                    TBagAnimatedTextureRef animRef;
                    auto                   spr = std::make_unique<TSpriteComponent>( resolveTexture( p_j, p_ctx, &animRef ) );
                    spr->m_animRef             = animRef;
                    if ( !animRef.empty() ) spr->m_textureRef = TBagTextureRef{ animRef.m_path };
                    spr->m_size     = jsonToVec2( p_j.value( "size", json::array() ), spr->m_size );
                    spr->m_color    = jsonToVec4( p_j.value( "color", json::array() ), spr->m_color );
                    spr->m_uvMin    = jsonToVec2( p_j.value( "uvMin", json::array() ), spr->m_uvMin );
                    spr->m_uvMax    = jsonToVec2( p_j.value( "uvMax", json::array() ), spr->m_uvMax );
                    spr->m_rotation = p_j.value( "rotation", spr->m_rotation );
                    spr->m_mode     = Reflect::enumParseLowerOr<TBillboardMode>( p_j.value( "mode", std::string( "spherical" ) ), TBillboardMode::Spherical );
                    spr->m_visible  = p_j.value( "visible", true );
                    spr->m_alphaMode =
                            Reflect::enumParseLowerOr<TSpriteAlphaMode>( p_j.value( "alphaMode", std::string( "cutout" ) ), TSpriteAlphaMode::Cutout );
                    spr->m_alphaCutoff = p_j.value( "alphaCutoff", spr->m_alphaCutoff );
                    return spr;
                } ) );

        registerType( Reflect::makeComponentStub<TParticleEmitterComponent>(
                []( const TComponent& p_c, const TComponentResolveCtx& p_ctx )
                {
                    const auto& p       = dynamic_cast<const TParticleEmitterComponent&>( p_c );
                    json        j       = Reflect::saveFields( p );
                    auto        texPath = texturePathJson( p.m_texture, p_ctx );
                    if ( !p.m_animRef.empty() )
                    {
                        j[ "texturePath" ] = p.m_animRef.m_path;
                        writeAnimFields( j, p.m_animRef );
                    }
                    else if ( !texPath.is_null() )
                    {
                        j[ "texturePath" ] = texPath;
                    }
                    return j;
                },
                []( const json& p_j, TComponentResolveCtx& p_ctx )
                {
                    auto p = std::make_unique<TParticleEmitterComponent>();
                    Reflect::loadFields( *p, p_j );
                    TBagAnimatedTextureRef animRef;
                    p->m_texture = resolveTexture( p_j, p_ctx, &animRef );
                    p->m_animRef = animRef;
                    return p;
                } ) );

        registerType( Reflect::makeComponentStub<TAudioComponent>(
                "audio", "Audio",
                []( const TComponent& p_c, const TComponentResolveCtx& p_ctx )
                {
                    const auto& a = dynamic_cast<const TAudioComponent&>( p_c );
                    json j{ { "volume", a.m_volume },   { "pitch", a.m_pitch },     { "minDistance", a.m_minDistance }, { "maxDistance", a.m_maxDistance },
                            { "looping", a.m_looping }, { "spatial", a.m_spatial }, { "playOnStart", a.m_playOnStart } };
                    if ( a.m_clip != nullptr )
                    {
                        if ( const auto* path = p_ctx.m_bag.findClipPath( a.m_clip ) )
                            j[ "clipPath" ] = *path;
                        else if ( !a.m_clip->m_path.empty() )
                            j[ "clipPath" ] = a.m_clip->m_path;
                        j[ "clipName" ] = a.m_clip->m_name;
                    }
                    return j;
                },
                []( const json& p_j, TComponentResolveCtx& p_ctx )
                {
                    const TAudioClip* clip = nullptr;
                    const std::string path = p_j.value( "clipPath", std::string{} );
                    if ( !path.empty() ) clip = p_ctx.m_bag.getOrCreateClip( path, p_j.value( "clipName", std::string{} ) );
                    auto a           = std::make_unique<TAudioComponent>( clip );
                    a->m_volume      = p_j.value( "volume", a->m_volume );
                    a->m_pitch       = p_j.value( "pitch", a->m_pitch );
                    a->m_minDistance = p_j.value( "minDistance", a->m_minDistance );
                    a->m_maxDistance = p_j.value( "maxDistance", a->m_maxDistance );
                    a->m_looping     = p_j.value( "looping", a->m_looping );
                    a->m_spatial     = p_j.value( "spatial", a->m_spatial );
                    a->m_playOnStart = p_j.value( "playOnStart", a->m_playOnStart );
                    return a;
                } ) );

        registerType( Reflect::makePodComponent<TRigidBodyComponent>( []( TRigidBodyComponent& p_b ) { p_b.setMass( p_b.m_mass ); } ) );

        registerType( Reflect::makePodComponent<TColliderComponent>() );

        registerType( Reflect::makeComponentStub<TScriptComponent>(
                "script", "Script",
                []( const TComponent& p_c, const TComponentResolveCtx& )
                {
                    const auto&       sc         = dynamic_cast<const TScriptComponent&>( p_c );
                    const std::string scriptType = sc.typeName();
                    if ( scriptType.empty() ) return json( nullptr );
                    return json{ { "script", scriptType } };
                },
                []( const json& p_j, TComponentResolveCtx& ) -> std::unique_ptr<TComponent>
                {
                    const std::string scriptType = p_j.value( "script", std::string{} );
                    if ( scriptType.empty() ) return {};
                    return TScriptComponent::makeFromType( scriptType );
                },
                nullptr ) );
    }
}  // namespace Tomos
