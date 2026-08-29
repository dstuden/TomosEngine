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
#include "Tomos/systems/asset/TAssetLoadQueue.hh"
#include "Tomos/systems/audio/TAudioComponent.hh"
#include "Tomos/systems/audio/TAudioSystem.hh"
#include "Tomos/systems/mesh/TMeshComponent.hh"
#include "Tomos/systems/particle/TParticleEmitterComponent.hh"
#include "Tomos/systems/sprite/TSpriteComponent.hh"
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
                const auto a = static_cast<unsigned char>( p_s[ p_s.size() - p_suffix.size() + i ] );
                const auto b = static_cast<unsigned char>( p_suffix[ i ] );
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
                 endsWithIgnoreCase( p_path, ".ktx2" ) || endsWithIgnoreCase( p_path, ".gif" ) || endsWithIgnoreCase( p_path, ".webp" ) ||
                 endsWithIgnoreCase( p_path, ".mp4" ) || endsWithIgnoreCase( p_path, ".webm" ) || endsWithIgnoreCase( p_path, ".mov" ) ||
                 endsWithIgnoreCase( p_path, ".mkv" ) || endsWithIgnoreCase( p_path, ".avi" ) )
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
            if ( app.gpu() == nullptr ) return false;

            TGpuAsset* asset = app.assetSystem().findByPath( p_path );
            if ( asset == nullptr ) asset = app.assetSystem().maybeGetAsset( p_path );

            const std::string stem = std::filesystem::path( p_path ).stem().string();
            TMeshAssetRef     ref{ stem, 0, 0 };

            if ( asset != nullptr && !asset->m_meshes.empty() )
            {
                ref.m_assetName            = asset->m_name;
                const TVkMesh*     mesh    = asset->mesh( 0 );
                const TVkMaterial* mat     = asset->m_materials.empty() ? nullptr : asset->material( 0 );
                const auto         gen     = app.assetSystem().generation();
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

            // Async: bind ref now (null mesh); rebind when upload finishes.
            const TAssetLoadHandle handle = app.assetLoadQueue().requestLoad( p_path, stem );
            if ( auto* existing = p_node.findComponent<TMeshComponent>() )
            {
                existing->m_ref             = ref;
                existing->m_boundGeneration = 0;
                existing->m_mesh            = nullptr;
                existing->m_material        = nullptr;
            }
            else
            {
                p_node.addComponent( std::make_shared<TMeshComponent>( ref, nullptr, nullptr, 0 ) );
            }

            app.assetLoadQueue().onComplete( handle,
                                             [ status = &p_ctx.m_status, path = p_path, handle ]( TAssetLoadStatus p_st )
                                             {
                                                 if ( p_st == TAssetLoadStatus::Ready )
                                                     *status = "Loaded mesh: " + path;
                                                 else
                                                     *status = "Load failed: " + path;
                                                 ( void ) handle;
                                             } );
            p_ctx.m_status = "Loading mesh: " + p_path;
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
            if ( p_ctx.m_bag == nullptr || p_ctx.m_scene == nullptr ) return false;
            TAudioClip* clip = p_ctx.m_bag->getOrCreateClip( p_path );
            p_ctx.m_scene->ecs().getSystem<TAudioSystem>().preload( clip );
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

            TBagAnimatedTextureRef opts{ p_path };
            TVkImage*              image = p_ctx.m_bag->resolveTexture( *gpu, p_path, opts );

            if ( auto* spr = p_node.findComponent<TSpriteComponent>() )
            {
                spr->m_texture    = image;
                spr->m_textureRef = TBagTextureRef{ p_path };
                spr->m_animRef    = opts;
                p_ctx.m_status    = "Assigned texture to sprite";
                return true;
            }
            if ( auto* part = p_node.findComponent<TParticleEmitterComponent>() )
            {
                part->m_texture = image;
                part->m_animRef = opts;
                p_ctx.m_status  = "Assigned texture to particles";
                return true;
            }
            if ( auto* mesh = p_node.findComponent<TMeshComponent>() )
            {
                mesh->m_baseTextureOverride = opts;
                mesh->rebindOverrides( *p_ctx.m_bag, *gpu );
                p_ctx.m_status = "Assigned base texture override to mesh";
                return true;
            }

            auto spr          = std::make_shared<TSpriteComponent>();
            spr->m_texture    = image;
            spr->m_textureRef = TBagTextureRef{ p_path };
            spr->m_animRef    = opts;
            p_node.addComponent( std::move( spr ) );
            p_ctx.m_status = "Added sprite with texture";
            return true;
        }

        void drawFileRow( const std::string& p_path )
        {
            const TAssetKind kind = classifyPath( p_path );
            const bool       anim = endsWithIgnoreCase( p_path, ".gif" ) || endsWithIgnoreCase( p_path, ".webp" ) || endsWithIgnoreCase( p_path, ".mp4" ) ||
                              endsWithIgnoreCase( p_path, ".webm" ) || endsWithIgnoreCase( p_path, ".mov" ) || endsWithIgnoreCase( p_path, ".mkv" ) ||
                              endsWithIgnoreCase( p_path, ".avi" );
            const char* tag = kind == TAssetKind::Mesh ? "[mesh]" : kind == TAssetKind::Audio ? "[audio]" : ( anim ? "[anim]" : "[tex]" );
            ImGui::Selectable( ( std::string( tag ) + "  " + p_path ).c_str() );
            if ( kind == TAssetKind::Mesh )
                beginDrag( TAssetBrowserPanel::g_kPayloadMesh, p_path, p_path.c_str() );
            else if ( kind == TAssetKind::Audio )
                beginDrag( TAssetBrowserPanel::g_kPayloadAudio, p_path, p_path.c_str() );
            else if ( kind == TAssetKind::Texture )
                beginDrag( TAssetBrowserPanel::g_kPayloadTexture, p_path, p_path.c_str() );
        }
    }  // namespace

    bool TAssetBrowserPanel::applyDrop( TSceneEditorContext& p_ctx, TSceneNode& p_node, const ImGuiPayload& p_payload )
    {
        if ( p_payload.Data == nullptr || p_payload.DataSize < 1 ) return false;
        const std::string data( static_cast<const char*>( p_payload.Data ) );

        if ( p_payload.IsDataType( g_kPayloadMesh ) ) return applyMeshPath( p_ctx, p_node, data );
        if ( p_payload.IsDataType( g_kPayloadGpuAsset ) ) return applyGpuAsset( p_ctx, p_node, data );
        if ( p_payload.IsDataType( g_kPayloadAudio ) ) return applyAudio( p_ctx, p_node, data );
        if ( p_payload.IsDataType( g_kPayloadTexture ) ) return applyTexture( p_ctx, p_node, data );
        return false;
    }

    void TAssetBrowserPanel::draw( TSceneEditorContext& /*p_ctx*/ )
    {
        ImGui::Begin( "Assets" );

        static char filterBuf[ 128 ] = {};
        ImGui::SetNextItemWidth( -1 );
        ImGui::InputTextWithHint( "##assetFilter", "Filter…", filterBuf, sizeof( filterBuf ) );
        const std::string filter = filterBuf;

        auto matches = [ & ]( const std::string& p_s )
        {
            if ( filter.empty() ) return true;
            auto lower = []( std::string p_v )
            {
                for ( char& c : p_v ) c = static_cast<char>( std::tolower( static_cast<unsigned char>( c ) ) );
                return p_v;
            };
            return lower( p_s ).find( lower( filter ) ) != std::string::npos;
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
                    TGpuAsset*  asset = TApplication::get().assetSystem().maybeGetAsset( name );
                    const char* hint  = ( asset != nullptr && !asset->m_sourcePath.empty() ) ? asset->m_sourcePath.c_str() : name.c_str();
                    ImGui::Selectable( ( std::string( "[gpu]  " ) + name ).c_str() );
                    if ( ImGui::IsItemHovered() && asset != nullptr )
                        ImGui::SetTooltip( "%s\n%zu meshes, %zu materials, %zu clips", hint, asset->m_meshes.size(), asset->m_materials.size(),
                                           asset->m_clips.size() );
                    beginDrag( g_kPayloadGpuAsset, name, name.c_str() );
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
        ImGui::End();
    }
}  // namespace Tomos
