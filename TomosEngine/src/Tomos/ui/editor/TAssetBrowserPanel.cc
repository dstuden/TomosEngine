#include "Tomos/ui/editor/TAssetBrowserPanel.hh"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <imgui.h>
#include <string>
#include <vector>

#include "Tomos/core/app/TApplication.hh"
#include "Tomos/core/scene/TSceneNode.hh"
#include "Tomos/gpu/vulkan/TVkGpu.hh"
#include "Tomos/systems/asset/TAssetSystem.hh"
#include "Tomos/systems/audio/TAudioComponent.hh"
#include "Tomos/systems/mesh/TMeshComponent.hh"
#include "Tomos/systems/particle/TParticleEmitterComponent.hh"
#include "Tomos/systems/sprite/TSpriteComponent.hh"
#include "Tomos/systems/asset/TGltfLoader.hh"
#include "Tomos/util/logger/TLogger.hh"
#include "Tomos/util/path/TPath.hh"

namespace Tomos
{
    namespace
    {
        enum class TAssetKind
        {
            Mesh,
            Audio,
            Texture,
            Other,
        };

        bool endsWithIgnoreCase( const std::string& p_s, const std::string& p_suffix )
        {
            if ( p_s.size() < p_suffix.size() ) return false;
            for ( size_t i = 0; i < p_suffix.size(); ++i )
            {
                const unsigned char a = static_cast<unsigned char>( p_s[ p_s.size() - p_suffix.size() + i ] );
                const unsigned char b = static_cast<unsigned char>( p_suffix[ i ] );
                if ( std::tolower( a ) != std::tolower( b ) ) return false;
            }
            return true;
        }

        TAssetKind classifyPath( const std::string& p_path )
        {
            if ( endsWithIgnoreCase( p_path, ".glb" ) || endsWithIgnoreCase( p_path, ".gltf" ) ) return TAssetKind::Mesh;
            if ( endsWithIgnoreCase( p_path, ".wav" ) || endsWithIgnoreCase( p_path, ".ogg" ) || endsWithIgnoreCase( p_path, ".mp3" ) ||
                 endsWithIgnoreCase( p_path, ".flac" ) )
                return TAssetKind::Audio;
            if ( endsWithIgnoreCase( p_path, ".png" ) || endsWithIgnoreCase( p_path, ".jpg" ) || endsWithIgnoreCase( p_path, ".jpeg" ) ||
                 endsWithIgnoreCase( p_path, ".tga" ) || endsWithIgnoreCase( p_path, ".bmp" ) || endsWithIgnoreCase( p_path, ".ktx" ) ||
                 endsWithIgnoreCase( p_path, ".ktx2" ) )
                return TAssetKind::Texture;
            return TAssetKind::Other;
        }

        std::string logicalPath( const std::filesystem::path& p_abs )
        {
            std::error_code ec;
            auto            rel = std::filesystem::relative( p_abs, TPath::assetRoot(), ec );
            if ( !ec && !rel.empty() && rel.native().find( ".." ) == std::string::npos ) return rel.generic_string();
            return p_abs.generic_string();
        }

        void collectFiles( const std::filesystem::path& p_dir, std::vector<std::string>& p_out )
        {
            std::error_code ec;
            if ( !std::filesystem::exists( p_dir, ec ) ) return;

            for ( const auto& entry : std::filesystem::recursive_directory_iterator( p_dir, std::filesystem::directory_options::skip_permission_denied, ec ) )
            {
                if ( ec )
                {
                    ec.clear();
                    continue;
                }
                if ( !entry.is_regular_file( ec ) ) continue;
                const auto kind = classifyPath( entry.path().string() );
                if ( kind == TAssetKind::Other ) continue;
                p_out.push_back( logicalPath( entry.path() ) );
            }
            std::sort( p_out.begin(), p_out.end() );
        }

        void beginDrag( const char* p_payloadType, const std::string& p_data, const char* p_preview )
        {
            if ( !ImGui::BeginDragDropSource( ImGuiDragDropFlags_SourceAllowNullID ) ) return;
            ImGui::SetDragDropPayload( p_payloadType, p_data.c_str(), p_data.size() + 1 );
            ImGui::Text( "%s", p_preview != nullptr ? p_preview : p_data.c_str() );
            ImGui::EndDragDropSource();
        }

        bool applyMeshPath( TSceneEditorContext& p_ctx, TSceneNode& p_node, const std::string& p_path )
        {
            auto& app = TApplication::get();
            auto* gpu = app.gpu();
            if ( gpu == nullptr ) return false;

            TGpuAsset* asset = app.assetSystem().findByPath( p_path );
            if ( asset == nullptr ) asset = app.assetSystem().maybeGetAsset( p_path );
            if ( asset == nullptr )
            {
                auto result = TGltfLoader::load( p_path, *gpu );
                if ( result.m_asset == nullptr )
                {
                    TLOG_ERROR() << "[TAssetBrowser] Failed to load mesh: " << p_path;
                    p_ctx.m_status = "Load failed: " + p_path;
                    return false;
                }
                if ( result.m_asset->m_name.empty() ) result.m_asset->m_name = std::filesystem::path( p_path ).stem().string();
                result.m_asset->m_sourcePath = p_path;
                result.m_asset->m_id         = TAssetSystem::makeStableId( p_path );
                asset                       = result.m_asset.get();
                app.assetSystem().registerAsset( std::move( result.m_asset ) );
                ( void ) result.m_root;
            }

            if ( asset->m_meshes.empty() )
            {
                p_ctx.m_status = "Asset has no meshes: " + asset->m_name;
                return false;
            }

            const TVkMesh*     mesh = asset->mesh( 0 );
            const TVkMaterial* mat  = asset->m_materials.empty() ? nullptr : asset->material( 0 );
            TMeshAssetRef      ref{ asset->m_name, 0, 0 };
            const auto         gen = TApplication::get().assetSystem().generation();

            if ( auto* existing = p_node.findComponent<TMeshComponent>() )
            {
                existing->m_ref             = ref;
                existing->m_boundGeneration = gen;
                existing->m_mesh            = mesh;
                existing->m_material        = mat;
            }
            else
            {
                p_node.addComponent( std::make_shared<TMeshComponent>( ref, mesh, mat, gen ) );
            }
            p_ctx.m_status = "Assigned mesh from " + asset->m_name;
            return true;
        }

        bool applyGpuAsset( TSceneEditorContext& p_ctx, TSceneNode& p_node, const std::string& p_name )
        {
            TGpuAsset* asset = TApplication::get().assetSystem().maybeGetAsset( p_name );
            if ( asset == nullptr || asset->m_meshes.empty() )
            {
                p_ctx.m_status = "GPU asset missing/empty: " + p_name;
                return false;
            }
            const TVkMesh*     mesh = asset->mesh( 0 );
            const TVkMaterial* mat  = asset->m_materials.empty() ? nullptr : asset->material( 0 );
            TMeshAssetRef      ref{ asset->m_name, 0, 0 };
            const auto         gen = TApplication::get().assetSystem().generation();
            if ( auto* existing = p_node.findComponent<TMeshComponent>() )
            {
                existing->m_ref             = ref;
                existing->m_boundGeneration = gen;
                existing->m_mesh            = mesh;
                existing->m_material        = mat;
            }
            else
            {
                p_node.addComponent( std::make_shared<TMeshComponent>( ref, mesh, mat, gen ) );
            }
            p_ctx.m_status = "Assigned mesh from " + p_name;
            return true;
        }

        bool applyAudio( TSceneEditorContext& p_ctx, TSceneNode& p_node, const std::string& p_path )
        {
            if ( p_ctx.m_bag == nullptr ) return false;
            TAudioClip* clip = p_ctx.m_bag->getOrCreateClip( p_path );
            if ( clip == nullptr )
            {
                p_ctx.m_status = "Audio load failed: " + p_path;
                return false;
            }
            if ( auto* existing = p_node.findComponent<TAudioComponent>() )
                existing->m_clip = clip;
            else
                p_node.addComponent( std::make_shared<TAudioComponent>( clip ) );
            p_ctx.m_status = "Assigned audio " + p_path;
            return true;
        }

        bool applyTexture( TSceneEditorContext& p_ctx, TSceneNode& p_node, const std::string& p_path )
        {
            auto* gpu = TApplication::get().gpu();
            if ( p_ctx.m_bag == nullptr || gpu == nullptr ) return false;
            TVkImage* image = p_ctx.m_bag->loadImage( *gpu, p_path );

            if ( auto* spr = p_node.findComponent<TSpriteComponent>() )
            {
                spr->m_texture    = image;
                spr->m_textureRef = TBagTextureRef{ p_path };
                p_ctx.m_status    = "Assigned texture to sprite";
                return true;
            }
            if ( auto* part = p_node.findComponent<TParticleEmitterComponent>() )
            {
                part->m_texture = image;
                p_ctx.m_status  = "Assigned texture to particles";
                return true;
            }

            auto spr          = std::make_shared<TSpriteComponent>();
            spr->m_texture    = image;
            spr->m_textureRef = TBagTextureRef{ p_path };
            p_node.addComponent( std::move( spr ) );
            p_ctx.m_status = "Added sprite with texture";
            return true;
        }

        void drawFileRow( const std::string& p_path )
        {
            const TAssetKind kind = classifyPath( p_path );
            const char*      tag  = kind == TAssetKind::Mesh ? "[mesh]" : kind == TAssetKind::Audio ? "[audio]" : "[tex]";
            ImGui::Selectable( ( std::string( tag ) + "  " + p_path ).c_str() );
            if ( kind == TAssetKind::Mesh )
                beginDrag( TAssetBrowserPanel::k_payloadMesh, p_path, p_path.c_str() );
            else if ( kind == TAssetKind::Audio )
                beginDrag( TAssetBrowserPanel::k_payloadAudio, p_path, p_path.c_str() );
            else if ( kind == TAssetKind::Texture )
                beginDrag( TAssetBrowserPanel::k_payloadTexture, p_path, p_path.c_str() );
        }
    }  // namespace

    bool TAssetBrowserPanel::applyDrop( TSceneEditorContext& p_ctx, TSceneNode& p_node, const ImGuiPayload& p_payload )
    {
        if ( p_payload.Data == nullptr || p_payload.DataSize < 1 ) return false;
        const std::string data( static_cast<const char*>( p_payload.Data ) );

        if ( p_payload.IsDataType( k_payloadMesh ) ) return applyMeshPath( p_ctx, p_node, data );
        if ( p_payload.IsDataType( k_payloadGpuAsset ) ) return applyGpuAsset( p_ctx, p_node, data );
        if ( p_payload.IsDataType( k_payloadAudio ) ) return applyAudio( p_ctx, p_node, data );
        if ( p_payload.IsDataType( k_payloadTexture ) ) return applyTexture( p_ctx, p_node, data );
        return false;
    }

    void TAssetBrowserPanel::draw( TSceneEditorContext& p_ctx )
    {
        ImGui::Begin( "Assets" );

        static char filterBuf[ 128 ] = {};
        ImGui::SetNextItemWidth( -1 );
        ImGui::InputTextWithHint( "##assetFilter", "Filter…", filterBuf, sizeof( filterBuf ) );
        const std::string filter = filterBuf;

        auto matches = [ & ]( const std::string& s )
        {
            if ( filter.empty() ) return true;
            auto lower = []( std::string v )
            {
                for ( char& c : v ) c = static_cast<char>( std::tolower( static_cast<unsigned char>( c ) ) );
                return v;
            };
            return lower( s ).find( lower( filter ) ) != std::string::npos;
        };

        if ( ImGui::CollapsingHeader( "Loaded (GPU)", ImGuiTreeNodeFlags_DefaultOpen ) )
        {
            auto names = TApplication::get().assetSystem().assetNames();
            std::sort( names.begin(), names.end() );
            if ( names.empty() )
                ImGui::TextDisabled( "No registered GPU assets" );
            else
            {
                for ( const auto& name : names )
                {
                    if ( !matches( name ) ) continue;
                    TGpuAsset* asset = TApplication::get().assetSystem().maybeGetAsset( name );
                    const char* hint = ( asset != nullptr && !asset->m_sourcePath.empty() ) ? asset->m_sourcePath.c_str() : name.c_str();
                    ImGui::Selectable( ( std::string( "[gpu]  " ) + name ).c_str() );
                    if ( ImGui::IsItemHovered() && asset != nullptr )
                        ImGui::SetTooltip( "%s\n%zu meshes, %zu materials, %zu clips", hint, asset->m_meshes.size(), asset->m_materials.size(),
                                           asset->m_clips.size() );
                    beginDrag( k_payloadGpuAsset, name, name.c_str() );
                }
            }
        }

        if ( ImGui::CollapsingHeader( "Filesystem", ImGuiTreeNodeFlags_DefaultOpen ) )
        {
            static std::vector<std::string> files;
            static bool                     scanned = false;
            if ( ImGui::Button( "Refresh" ) ) scanned = false;
            ImGui::SameLine();
            ImGui::TextDisabled( "%s", TPath::assetRoot().generic_string().c_str() );

            if ( !scanned )
            {
                files.clear();
                collectFiles( TPath::assetRoot() / "assets", files );
                if ( files.empty() ) collectFiles( TPath::assetRoot(), files );
                scanned = true;
            }

            if ( files.empty() )
                ImGui::TextDisabled( "No mesh/audio/texture files under asset root" );
            else
            {
                ImGui::BeginChild( "##assetFiles", ImVec2( 0, 0 ), ImGuiChildFlags_None );
                for ( const auto& path : files )
                {
                    if ( !matches( path ) ) continue;
                    drawFileRow( path );
                }
                ImGui::EndChild();
            }
        }

        ImGui::TextDisabled( "Drag onto Hierarchy / Inspector nodes" );
        ( void ) p_ctx;
        ImGui::End();
    }
}  // namespace Tomos
