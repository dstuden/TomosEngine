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
#include "Tomos/systems/mesh/TMeshComponent.hh"
#include "Tomos/systems/asset/TGltfLoader.hh"
#include "Tomos/util/logger/TLogger.hh"
#include "Tomos/util/path/TPath.hh"

namespace Tomos
{
    using json = nlohmann::json;

    namespace
    {
        json transformToJson( const TTransform& t )
        {
            return json{ { "t", json::array( { t.translation().x, t.translation().y, t.translation().z } ) },
                         { "r", json::array( { t.rotation().x, t.rotation().y, t.rotation().z, t.rotation().w } ) },
                         { "s", json::array( { t.scale().x, t.scale().y, t.scale().z } ) } };
        }

        void transformFromJson( TTransform& t, const json& j )
        {
            glm::vec3 translation = t.translation();
            glm::quat rotation    = t.rotation();
            glm::vec3 scale       = t.scale();
            if ( j.contains( "t" ) && j[ "t" ].is_array() && j[ "t" ].size() >= 3 )
                translation = { j[ "t" ][ 0 ].get<float>(), j[ "t" ][ 1 ].get<float>(), j[ "t" ][ 2 ].get<float>() };
            if ( j.contains( "r" ) && j[ "r" ].is_array() && j[ "r" ].size() >= 4 )
                rotation = glm::quat( j[ "r" ][ 3 ].get<float>(), j[ "r" ][ 0 ].get<float>(), j[ "r" ][ 1 ].get<float>(), j[ "r" ][ 2 ].get<float>() );
            if ( j.contains( "s" ) && j[ "s" ].is_array() && j[ "s" ].size() >= 3 )
                scale = { j[ "s" ][ 0 ].get<float>(), j[ "s" ][ 1 ].get<float>(), j[ "s" ][ 2 ].get<float>() };
            t.setLocalTRS( translation, rotation, scale );
        }

        json saveNode( const TSceneNode& node, const TComponentResolveCtx& ctx )
        {
            json j;
            j[ "id" ]        = node.m_id;
            j[ "name" ]      = node.m_name;
            j[ "dynamic" ]   = node.m_dynamic;
            j[ "transform" ] = transformToJson( node.m_transform );

            json comps = json::array();
            for ( const auto& c : node.getComponents() )
            {
                json cj = TComponentRegistry::get().saveComponent( *c, ctx );
                if ( !cj.is_null() ) comps.push_back( std::move( cj ) );
            }
            j[ "components" ] = std::move( comps );

            json children = json::array();
            for ( const auto& child : node.getChildren() ) children.push_back( saveNode( *child, ctx ) );
            j[ "children" ] = std::move( children );
            return j;
        }

        void collectUsedAssets( const TSceneNode& node, const TAssetSystem& assets, std::unordered_set<std::string>& used )
        {
            for ( const auto& c : node.getComponents() )
            {
                if ( const auto* mesh = dynamic_cast<const TMeshComponent*>( c.get() ) )
                {
                    TMeshAssetRef ref{};
                    if ( assets.resolveMesh( mesh->m_mesh, mesh->m_material, ref ) ) used.insert( ref.m_assetName );
                }
                if ( const auto* anim = dynamic_cast<const TAnimatorComponent*>( c.get() ) )
                {
                    std::string assetName, clipName;
                    if ( anim->m_clip != nullptr && assets.resolveClip( anim->m_clip, assetName, clipName ) ) used.insert( assetName );
                }
            }
            for ( const auto& child : node.getChildren() ) collectUsedAssets( *child, assets, used );
        }

        std::shared_ptr<TSceneNode> loadNode( const json& j, TComponentResolveCtx& ctx, std::unordered_map<uint64_t, std::shared_ptr<TSceneNode>>& byId )
        {
            auto node = std::make_shared<TSceneNode>( j.value( "name", std::string( "<unnamed>" ) ) );
            if ( j.contains( "id" ) ) node->setId( j[ "id" ].get<uint64_t>() );
            node->m_dynamic = j.value( "dynamic", true );
            if ( j.contains( "transform" ) ) transformFromJson( node->m_transform, j[ "transform" ] );

            byId[ node->m_id ] = node;

            if ( j.contains( "components" ) && j[ "components" ].is_array() )
            {
                for ( const auto& cj : j[ "components" ] )
                {
                    if ( auto comp = TComponentRegistry::get().loadComponent( cj, ctx ) ) node->addComponent( std::move( comp ) );
                }
            }

            if ( j.contains( "script" ) && j[ "script" ].is_string() )
            {
                const std::string scriptType = j[ "script" ].get<std::string>();
                if ( !scriptType.empty() )
                {
                    json scriptJson = json{ { "type", "script" }, { "script", scriptType } };
                    if ( auto comp = TComponentRegistry::get().loadComponent( scriptJson, ctx ) ) node->addComponent( std::move( comp ) );
                }
            }

            if ( j.contains( "children" ) && j[ "children" ].is_array() )
            {
                for ( const auto& childJ : j[ "children" ] )
                {
                    if ( auto child = loadNode( childJ, ctx, byId ) ) node->addChild( std::move( child ) );
                }
            }
            return node;
        }

    }  // namespace

    bool TSceneSerializer::saveToFile( const TScene& p_scene, const TAssetSystem& p_assets, const std::string& p_path )
    {
        const std::string resolvedPath = TPath::resolveString( p_path );

        std::unordered_set<std::string> usedAssets;
        for ( const auto& child : p_scene.getChildren() ) collectUsedAssets( *child, p_assets, usedAssets );

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

        TComponentResolveCtx ctx{ const_cast<TAssetSystem&>( p_assets ), const_cast<TSceneResourceBag&>( p_scene.resources() ), nullptr };
        json                 children = json::array();
        for ( const auto& child : p_scene.getChildren() ) children.push_back( saveNode( *child, ctx ) );
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

    bool TSceneSerializer::loadFromFile( TScene& p_scene, TAssetSystem& p_assets, TVkGpu& p_gpu, const std::string& p_path, const TSceneSerializeOpts& p_opts )
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
                const std::string assetPath = TPath::resolveString( path );
                auto              result    = TGltfLoader::load( path, p_gpu );
                if ( result.m_asset == nullptr )
                {
                    TLOG_ERROR() << "[TSceneSerializer] Failed to load asset: " << assetPath;
                    continue;
                }
                result.m_asset->m_name       = name;
                result.m_asset->m_sourcePath = path;
                result.m_asset->m_id         = TAssetSystem::makeStableId( path );
                p_assets.registerAsset( std::move( result.m_asset ) );
            }
        }

        if ( p_opts.m_replaceChildren )
        {
            p_scene.clearChildren();
            p_scene.resources().clear();
        }

        std::unordered_map<uint64_t, std::shared_ptr<TSceneNode>> byId;
        std::vector<TPendingSkinnedJoints>                        pendingSkinned;

        TComponentResolveCtx ctx{ p_assets, p_scene.resources(), &p_gpu, &byId, &pendingSkinned };

        if ( doc.contains( "children" ) && doc[ "children" ].is_array() )
        {
            for ( const auto& childJ : doc[ "children" ] )
            {
                if ( auto child = loadNode( childJ, ctx, byId ) ) p_scene.addChild( std::move( child ) );
            }
        }
        else if ( doc.contains( "root" ) && doc[ "root" ].is_object() )
        {
            const auto& rootJ = doc[ "root" ];
            if ( rootJ.contains( "children" ) && rootJ[ "children" ].is_array() )
            {
                for ( const auto& childJ : rootJ[ "children" ] )
                {
                    if ( auto child = loadNode( childJ, ctx, byId ) ) p_scene.addChild( std::move( child ) );
                }
            }
        }

        TComponentRegistry::wireSkinnedJoints( pendingSkinned, byId );
        TLOG_INFO() << "[TSceneSerializer] Loaded scene ← " << resolvedPath << " (" << byId.size() << " nodes)";
        return true;
    }
}  // namespace Tomos
