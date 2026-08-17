#pragma once

#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

#include "Tomos/gpu/vulkan/TVkMaterial.hh"
#include "Tomos/gpu/vulkan/TVkMesh.hh"
#include "Tomos/systems/animation/TAnimationClip.hh"
#include "Tomos/systems/asset/TAssetHandles.hh"
#include "Tomos/util/logger/TLogger.hh"
#include "Tomos/util/path/TPath.hh"

namespace Tomos
{
    // Named glTF package — sole owner of meshes/materials/textures/clips.
    // Ownership split: see TAssetHandles.hh.
    struct TGpuAsset
    {
        std::string m_name;
        // Prefer m_id over m_name for persistence (asset-root-relative when known).
        std::string                                  m_id;
        std::string                                  m_sourcePath;
        std::vector<std::unique_ptr<TVkMesh>>        m_meshes;
        std::vector<std::unique_ptr<TVkMaterial>>    m_materials;
        std::vector<std::unique_ptr<TVkImage>>       m_textures;
        std::vector<std::unique_ptr<TAnimationClip>> m_clips;

        [[nodiscard]] const TVkMesh*        mesh( uint32_t p_idx ) const { return m_meshes.at( p_idx ).get(); }
        [[nodiscard]] const TVkMaterial*    material( uint32_t p_idx ) const { return m_materials.at( p_idx ).get(); }
        [[nodiscard]] const TAnimationClip* clip( uint32_t p_idx ) const { return m_clips.at( p_idx ).get(); }
        [[nodiscard]] const TAnimationClip* findClip( const std::string& p_name ) const
        {
            for ( const auto& c : m_clips )
                if ( c->m_name == p_name ) return c.get();
            TLOG_WARN() << "[TGpuAsset] Clip not found: '" << p_name << "' in asset '" << m_name << "'";
            return nullptr;
        }
    };

    // Global GPU asset registry (not an ECS system). Do not put sprite/particle
    // textures or audio here — use TScene::resources().
    class TAssetSystem
    {
    public:
        [[nodiscard]] static std::string makeStableId( const std::string& p_pathOrName )
        {
            if ( p_pathOrName.empty() ) return {};

            std::filesystem::path abs = TPath::resolve( p_pathOrName );
            std::error_code       ec;
            const auto&           root = TPath::assetRoot();
            auto                  rel  = std::filesystem::relative( abs, root, ec );
            std::filesystem::path use  = ( !ec && !rel.empty() && rel.native().find( ".." ) == std::string::npos ) ? rel : abs;

            std::string out = use.generic_string();
            return out;
        }

        // Bumps on clear and asset replace; compare against m_boundGeneration.
        [[nodiscard]] TResourceGeneration generation() const { return m_generation; }

        // Replaces same name (generation bump) — callers with cached pointers must rebind.
        void registerAsset( std::unique_ptr<TGpuAsset> p_asset )
        {
            if ( p_asset == nullptr ) return;

            if ( p_asset->m_id.empty() )
            {
                if ( !p_asset->m_sourcePath.empty() )
                    p_asset->m_id = makeStableId( p_asset->m_sourcePath );
                else
                    p_asset->m_id = p_asset->m_name;
            }

            if ( !p_asset->m_sourcePath.empty() )
            {
                const std::string normalized = makeStableId( p_asset->m_sourcePath );
                if ( !normalized.empty() ) p_asset->m_sourcePath = normalized;
            }

            const std::string name = p_asset->m_name;
            if ( name.empty() ) throw std::runtime_error( "[TAssetSystem] Cannot register asset with empty name" );

            if ( m_assets.contains( name ) )
            {
                TLOG_WARN() << "[TAssetSystem] Replacing asset '" << name << "' (generation bump)";
                erasePathIndexFor( name );
                bumpGeneration();
            }

            TGpuAsset* raw   = p_asset.get();
            m_assets[ name ] = std::move( p_asset );
            indexPaths( *raw );
        }

        [[nodiscard]] TGpuAsset* maybeGetAsset( const std::string& p_name ) const
        {
            const auto it = m_assets.find( p_name );
            return ( it != m_assets.end() ) ? it->second.get() : nullptr;
        }

        [[nodiscard]] TGpuAsset* findByPath( const std::string& p_path ) const
        {
            if ( p_path.empty() ) return nullptr;
            const std::string id = makeStableId( p_path );
            if ( auto it = m_byPath.find( id ); it != m_byPath.end() ) return it->second;
            if ( auto it = m_byPath.find( p_path ); it != m_byPath.end() ) return it->second;
            return nullptr;
        }

        [[nodiscard]] TGpuAsset* find( const std::string& p_nameOrPath ) const
        {
            if ( TGpuAsset* byName = maybeGetAsset( p_nameOrPath ) ) return byName;
            return findByPath( p_nameOrPath );
        }

        [[nodiscard]] TGpuAsset& getAsset( const std::string& p_name ) const
        {
            TGpuAsset* ptr = maybeGetAsset( p_name );
            if ( ptr == nullptr ) throw std::runtime_error( "[TAssetSystem] Asset not found: " + p_name );
            return *ptr;
        }

        // Must run before VkDevice destruction.
        void clear()
        {
            if ( !m_assets.empty() ) bumpGeneration();
            m_byPath.clear();
            m_assets.clear();
        }

        [[nodiscard]] std::vector<std::string> assetNames() const
        {
            std::vector<std::string> names;
            names.reserve( m_assets.size() );
            for ( const auto& [ name, _ ] : m_assets ) names.push_back( name );
            return names;
        }

        template<typename Fn>
        void forEach( Fn&& p_fn ) const
        {
            for ( const auto& [ name, asset ] : m_assets ) p_fn( name, *asset );
        }

        [[nodiscard]] bool tryResolve( const TMeshAssetRef& p_ref, const TVkMesh*& p_mesh, const TVkMaterial*& p_material ) const
        {
            p_mesh     = nullptr;
            p_material = nullptr;
            if ( p_ref.empty() ) return false;
            TGpuAsset* asset = maybeGetAsset( p_ref.m_assetName );
            if ( asset == nullptr ) return false;
            if ( p_ref.m_meshIdx >= asset->m_meshes.size() ) return false;
            p_mesh = asset->mesh( p_ref.m_meshIdx );
            if ( p_ref.m_materialIdx < asset->m_materials.size() ) p_material = asset->material( p_ref.m_materialIdx );
            return p_mesh != nullptr;
        }

        [[nodiscard]] const TAnimationClip* tryResolve( const TClipAssetRef& p_ref ) const
        {
            if ( p_ref.empty() ) return nullptr;
            TGpuAsset* asset = maybeGetAsset( p_ref.m_assetName );
            if ( asset == nullptr ) return nullptr;
            if ( p_ref.m_clipName.empty() ) return asset->m_clips.empty() ? nullptr : asset->m_clips.front().get();
            return asset->findClip( p_ref.m_clipName );
        }

        [[nodiscard]] bool resolveMesh( const TVkMesh* p_mesh, const TVkMaterial* p_material, TMeshAssetRef& p_out ) const
        {
            for ( const auto& [ name, asset ] : m_assets )
            {
                int meshIdx = -1;
                for ( size_t i = 0; i < asset->m_meshes.size(); ++i )
                {
                    if ( asset->m_meshes[ i ].get() == p_mesh )
                    {
                        meshIdx = static_cast<int>( i );
                        break;
                    }
                }
                if ( meshIdx < 0 ) continue;

                int matIdx = 0;
                for ( size_t i = 0; i < asset->m_materials.size(); ++i )
                {
                    if ( asset->m_materials[ i ].get() == p_material )
                    {
                        matIdx = static_cast<int>( i );
                        break;
                    }
                }

                p_out.m_assetName   = name;
                p_out.m_meshIdx     = static_cast<uint32_t>( meshIdx );
                p_out.m_materialIdx = static_cast<uint32_t>( matIdx );
                return true;
            }
            return false;
        }

        [[nodiscard]] bool resolveClip( const TAnimationClip* p_clip, std::string& p_assetName, std::string& p_clipName ) const
        {
            TClipAssetRef ref;
            if ( !resolveClip( p_clip, ref ) ) return false;
            p_assetName = ref.m_assetName;
            p_clipName  = ref.m_clipName;
            return true;
        }

        [[nodiscard]] bool resolveClip( const TAnimationClip* p_clip, TClipAssetRef& p_out ) const
        {
            for ( const auto& [ name, asset ] : m_assets )
            {
                for ( const auto& clip : asset->m_clips )
                {
                    if ( clip.get() == p_clip )
                    {
                        p_out.m_assetName = name;
                        p_out.m_clipName  = clip->m_name;
                        return true;
                    }
                }
            }
            return false;
        }

    private:
        void bumpGeneration() { ++m_generation; }

        void indexPaths( TGpuAsset& p_asset )
        {
            if ( !p_asset.m_id.empty() ) m_byPath[ p_asset.m_id ] = &p_asset;
            if ( !p_asset.m_sourcePath.empty() ) m_byPath[ p_asset.m_sourcePath ] = &p_asset;
        }

        void erasePathIndexFor( const std::string& p_name )
        {
            TGpuAsset* old = maybeGetAsset( p_name );
            if ( old == nullptr ) return;
            for ( auto it = m_byPath.begin(); it != m_byPath.end(); )
            {
                if ( it->second == old )
                    it = m_byPath.erase( it );
                else
                    ++it;
            }
        }

        TResourceGeneration                                         m_generation = 1;
        std::unordered_map<std::string, std::unique_ptr<TGpuAsset>> m_assets;
        std::unordered_map<std::string, TGpuAsset*>                 m_byPath;
    };
}  // namespace Tomos
