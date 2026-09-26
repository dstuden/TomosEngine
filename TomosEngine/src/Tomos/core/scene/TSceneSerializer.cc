#include "Tomos/core/scene/TSceneSerializer.hh"

#include <filesystem>
#include <fstream>
#include <glm/glm.hpp>
#include <nlohmann/json.hpp>
#include <unordered_map>
#include <unordered_set>

#include "Tomos/gpu/vulkan/TVkGpu.hh"
#include "Tomos/systems/TComponentRegistry.hh"
#include "Tomos/systems/animation/TAnimatorComponent.hh"
#include "Tomos/systems/asset/TAssetLoadQueue.hh"
#include "Tomos/systems/mesh/TMeshComponent.hh"
#include "Tomos/util/logger/TLogger.hh"
#include "Tomos/util/path/TPath.hh"

namespace Tomos
{
    using json = nlohmann::json;

    namespace
    {
        json transformToJson( const TTransform& p_t )
        {
            return json{ { "t", json::array( { p_t.translation().x, p_t.translation().y, p_t.translation().z } ) },
                         { "r", json::array( { p_t.rotation().x, p_t.rotation().y, p_t.rotation().z, p_t.rotation().w } ) },
                         { "s", json::array( { p_t.scale().x, p_t.scale().y, p_t.scale().z } ) } };
        }

        void transformFromJson( TTransform& p_t, const json& p_j )
        {
            glm::vec3 translation = p_t.translation();
            glm::quat rotation    = p_t.rotation();
            glm::vec3 scale       = p_t.scale();
            if ( p_j.contains( "t" ) && p_j[ "t" ].is_array() && p_j[ "t" ].size() >= 3 )
                translation = { p_j[ "t" ][ 0 ].get<float>(), p_j[ "t" ][ 1 ].get<float>(), p_j[ "t" ][ 2 ].get<float>() };
            if ( p_j.contains( "r" ) && p_j[ "r" ].is_array() && p_j[ "r" ].size() >= 4 )
                rotation = glm::quat( p_j[ "r" ][ 3 ].get<float>(), p_j[ "r" ][ 0 ].get<float>(), p_j[ "r" ][ 1 ].get<float>(), p_j[ "r" ][ 2 ].get<float>() );
            if ( p_j.contains( "s" ) && p_j[ "s" ].is_array() && p_j[ "s" ].size() >= 3 )
                scale = { p_j[ "s" ][ 0 ].get<float>(), p_j[ "s" ][ 1 ].get<float>(), p_j[ "s" ][ 2 ].get<float>() };
            p_t.setLocalTRS( translation, rotation, scale );
        }

        json saveNode( const TSceneNode& p_node, const TComponentResolveCtx& p_ctx )
        {
            json j;
            j[ "id" ]        = p_node.m_id;
            j[ "name" ]      = p_node.m_name;
            j[ "dynamic" ]   = p_node.m_dynamic;
            j[ "transform" ] = transformToJson( p_node.m_transform );

            json comps = json::array();
            for ( TComponent* c : p_node.getComponents() )
            {
                if ( c == nullptr ) continue;
                json cj = TComponentRegistry::get().saveComponent( *c, p_ctx );
                if ( !cj.is_null() ) comps.push_back( std::move( cj ) );
            }
            j[ "components" ] = std::move( comps );

            json children = json::array();
            for ( TSceneNode* child : p_node.getChildren() )
            {
                if ( child != nullptr ) children.push_back( saveNode( *child, p_ctx ) );
            }
            j[ "children" ] = std::move( children );
            return j;
        }

        void collectUsedAssets( const TSceneNode& p_node, const TAssetSystem& p_assets, std::unordered_set<std::string>& p_used )
        {
            for ( TComponent* c : p_node.getComponents() )
            {
                if ( c == nullptr ) continue;
                if ( const auto* mesh = dynamic_cast<const TMeshComponent*>( c ) )
                {
                    TMeshAssetRef ref{};
                    if ( p_assets.resolveMesh( mesh->m_mesh, mesh->m_material, ref ) ) p_used.insert( ref.m_assetName );
                }
                if ( const auto* anim = dynamic_cast<const TAnimatorComponent*>( c ) )
                {
                    std::string assetName, clipName;
                    if ( anim->m_clip != nullptr && p_assets.resolveClip( anim->m_clip, assetName, clipName ) ) p_used.insert( assetName );
                }
            }
            for ( TSceneNode* child : p_node.getChildren() )
            {
                if ( child != nullptr ) collectUsedAssets( *child, p_assets, p_used );
            }
        }

        TSceneNode* loadNode( TScene& p_scene, const json& p_j, TComponentResolveCtx& p_ctx, std::unordered_map<uint64_t, TSceneNode*>& p_byId )
        {
            TSceneNode& node = p_scene.createNode( p_j.value( "name", std::string( "<unnamed>" ) ) );
            if ( p_j.contains( "id" ) ) node.setId( p_j[ "id" ].get<uint64_t>() );
            node.m_dynamic = p_j.value( "dynamic", true );
            if ( p_j.contains( "transform" ) ) transformFromJson( node.m_transform, p_j[ "transform" ] );

            p_byId[ node.m_id ] = &node;

            if ( p_j.contains( "components" ) && p_j[ "components" ].is_array() )
            {
                for ( const auto& cj : p_j[ "components" ] )
                {
                    if ( auto comp = TComponentRegistry::get().loadComponent( cj, p_ctx ) ) node.addComponent( std::move( comp ) );
                }
            }

            if ( p_j.contains( "script" ) && p_j[ "script" ].is_string() )
            {
                const std::string scriptType = p_j[ "script" ].get<std::string>();
                if ( !scriptType.empty() )
                {
                    json scriptJson = json{ { "type", "script" }, { "script", scriptType } };
                    if ( auto comp = TComponentRegistry::get().loadComponent( scriptJson, p_ctx ) ) node.addComponent( std::move( comp ) );
                }
            }

            if ( p_j.contains( "children" ) && p_j[ "children" ].is_array() )
            {
                for ( const auto& childJ : p_j[ "children" ] )
                {
                    if ( TSceneNode* child = loadNode( p_scene, childJ, p_ctx, p_byId ) ) node.addChild( child );
                }
            }
            return &node;
        }

    }  // namespace

    bool TSceneSerializer::saveToFile( const TScene& p_scene, const TAssetSystem& p_assets, const std::string& p_path )
    {
        const std::string resolvedPath = TPath::resolveString( p_path );

        std::unordered_set<std::string> usedAssets;
        for ( TSceneNode* child : p_scene.getChildren() )
        {
            if ( child != nullptr ) collectUsedAssets( *child, p_assets, usedAssets );
        }

        json doc;

        json assets = json::array();
        for ( const auto& name : usedAssets )
        {
            if ( const TGpuAsset* asset = p_assets.maybeGetAsset( name ) )
            {
                json a{ { "name", asset->m_name } };
                if ( !asset->m_id.empty() ) a[ "id" ] = asset->m_id;
                if ( !asset->m_sourcePath.empty() ) a[ "path" ] = asset->m_sourcePath;
                assets.push_back( std::move( a ) );
            }
        }
        doc[ "assets" ] = std::move( assets );

        TComponentResolveCtx ctx{ const_cast<TAssetSystem&>( p_assets ), const_cast<TSceneResourceBag&>( p_scene.resources() ), nullptr, nullptr,
                                  &p_scene.store() };
        json                 children = json::array();
        for ( TSceneNode* child : p_scene.getChildren() )
        {
            if ( child != nullptr ) children.push_back( saveNode( *child, ctx ) );
        }
        doc[ "children" ] = std::move( children );

        std::error_code ec;
        const auto      parent = std::filesystem::path( resolvedPath ).parent_path();
        if ( !parent.empty() ) std::filesystem::create_directories( parent, ec );

        std::ofstream out( resolvedPath );
        if ( !out )
        {
            TLOG_ERROR() << "[TSceneSerializer] Failed to write: " << resolvedPath;
            return false;
        }
        out << doc.dump( 2 );
        TLOG_INFO() << "[TSceneSerializer] Saved scene → " << resolvedPath;
        return true;
    }

    bool TSceneSerializer::loadFromFile( TScene& p_scene, TAssetSystem& p_assets, TVkGpu& p_gpu, TAssetLoadQueue& p_loads, const std::string& p_path,
                                          const TSceneSerializeOpts& p_opts )
    {
        const std::string resolvedPath = TPath::resolveString( p_path );

        std::ifstream in( resolvedPath );
        if ( !in )
        {
            TLOG_ERROR() << "[TSceneSerializer] Failed to open: " << resolvedPath;
            return false;
        }

        json doc;
        try
        {
            in >> doc;
        }
        catch ( const std::exception& e )
        {
            TLOG_ERROR() << "[TSceneSerializer] JSON parse error: " << e.what();
            return false;
        }

        if ( doc.contains( "assets" ) && doc[ "assets" ].is_array() )
        {
            for ( const auto& a : doc[ "assets" ] )
            {
                const std::string name = a.value( "name", std::string{} );
                const std::string path = a.value( "path", std::string{} );
                if ( name.empty() ) continue;
                if ( p_assets.maybeGetAsset( name ) != nullptr ) continue;
                if ( !path.empty() && p_assets.findByPath( path ) != nullptr ) continue;
                if ( path.empty() )
                {
                    TLOG_WARN() << "[TSceneSerializer] Asset '" << name << "' missing and no path to load";
                    continue;
                }
                // Non-blocking: mesh/clip components keep refs and rebind when ready.
                p_loads.requestLoad( path, name );
                TLOG_INFO() << "[TSceneSerializer] Queued asset load '" << name << "' from " << path;
            }
        }

        if ( p_opts.m_replaceChildren )
        {
            p_scene.clearChildren();
            p_scene.resources().clear();
        }

        std::unordered_map<uint64_t, TSceneNode*> byId;
        std::vector<TPendingSkinnedJoints>        pendingSkinned;

        TComponentResolveCtx ctx{ p_assets, p_scene.resources(), &p_gpu, &byId, &p_scene.store(), &pendingSkinned };

        if ( doc.contains( "children" ) && doc[ "children" ].is_array() )
        {
            for ( const auto& childJ : doc[ "children" ] )
            {
                if ( TSceneNode* child = loadNode( p_scene, childJ, ctx, byId ) ) p_scene.addChild( child );
            }
        }
        else if ( doc.contains( "root" ) && doc[ "root" ].is_object() )
        {
            const auto& rootJ = doc[ "root" ];
            if ( rootJ.contains( "children" ) && rootJ[ "children" ].is_array() )
            {
                for ( const auto& childJ : rootJ[ "children" ] )
                {
                    if ( TSceneNode* child = loadNode( p_scene, childJ, ctx, byId ) ) p_scene.addChild( child );
                }
            }
        }

        TComponentRegistry::wireSkinnedJoints( pendingSkinned, byId );
        TLOG_INFO() << "[TSceneSerializer] Loaded scene ← " << resolvedPath << " (" << byId.size() << " nodes)";
        return true;
    }
}  // namespace Tomos
