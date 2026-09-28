#include "Tomos/systems/asset/TGltfLoader.hh"

#include <algorithm>
#include <assimp/GltfMaterial.h>
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <cmath>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <limits>
#include <stb/stb_image.h>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

#include "Tomos/core/scene/TSceneNode.hh"
#include "Tomos/gpu/vulkan/TVkGpu.hh"
#include "Tomos/systems/animation/TAnimationClip.hh"
#include "Tomos/systems/mesh/TMeshComponent.hh"
#include "Tomos/util/logger/TLogger.hh"
#include "Tomos/util/memory/TLevelStore.hh"
#include "Tomos/util/path/TPath.hh"

namespace Tomos
{
    namespace
    {
        constexpr int32_t g_kTexDefault = -1;
        constexpr int32_t g_kTexMissing = -2;

        bool decodeTexturePixels( const aiScene* p_scene, const std::string& p_texPath, const std::string& p_baseDir, TCpuTextureData& p_out )
        {
            int      w = 0, h = 0, ch = 0;
            uint8_t* pixels   = nullptr;
            bool     stbOwned = true;

            if ( !p_texPath.empty() && p_texPath[ 0 ] == '*' )
            {
                const int        idx = std::atoi( p_texPath.c_str() + 1 );
                const aiTexture* tex = p_scene->mTextures[ idx ];

                if ( tex->mHeight == 0 )
                {
                    pixels = stbi_load_from_memory( reinterpret_cast<const stbi_uc*>( tex->pcData ), static_cast<int>( tex->mWidth ), &w, &h, &ch, 4 );
                }
                else
                {
                    w        = static_cast<int>( tex->mWidth );
                    h        = static_cast<int>( tex->mHeight );
                    pixels   = new uint8_t[ w * h * 4 ];
                    stbOwned = false;
                    for ( int i = 0; i < w * h; ++i )
                    {
                        pixels[ i * 4 + 0 ] = tex->pcData[ i ].r;
                        pixels[ i * 4 + 1 ] = tex->pcData[ i ].g;
                        pixels[ i * 4 + 2 ] = tex->pcData[ i ].b;
                        pixels[ i * 4 + 3 ] = tex->pcData[ i ].a;
                    }
                }
            }
            else
            {
                const std::string fullPath = p_baseDir + "/" + p_texPath;
                pixels                     = stbi_load( fullPath.c_str(), &w, &h, &ch, 4 );
            }

            if ( pixels == nullptr ) return false;

            p_out.m_width  = static_cast<uint32_t>( w );
            p_out.m_height = static_cast<uint32_t>( h );
            p_out.m_pixels.assign( pixels, pixels + static_cast<size_t>( w ) * h * 4 );

            if ( stbOwned )
                stbi_image_free( pixels );
            else
                delete[] pixels;

            return true;
        }

        glm::mat4 aiToGlm( const aiMatrix4x4& p_m )
        {
            return glm::mat4( p_m.a1, p_m.b1, p_m.c1, p_m.d1, p_m.a2, p_m.b2, p_m.c2, p_m.d2, p_m.a3, p_m.b3, p_m.c3, p_m.d3, p_m.a4, p_m.b4, p_m.c4, p_m.d4 );
        }

        TCpuMeshData extractMesh( const aiMesh* p_ai )
        {
            TCpuMeshData mesh{};
            mesh.m_materialIndex = p_ai->mMaterialIndex;

            {
                mesh.m_positions.resize( p_ai->mNumVertices );
                glm::vec3 bmin( std::numeric_limits<float>::max() );
                glm::vec3 bmax( std::numeric_limits<float>::lowest() );
                for ( unsigned i = 0; i < p_ai->mNumVertices; ++i )
                {
                    mesh.m_positions[ i ] = { p_ai->mVertices[ i ].x, p_ai->mVertices[ i ].y, p_ai->mVertices[ i ].z };
                    bmin                  = glm::min( bmin, mesh.m_positions[ i ] );
                    bmax                  = glm::max( bmax, mesh.m_positions[ i ] );
                }
                mesh.m_aabb.m_min = bmin;
                mesh.m_aabb.m_max = bmax;
            }

            if ( p_ai->HasTextureCoords( 0 ) )
            {
                mesh.m_uvs.resize( p_ai->mNumVertices );
                for ( unsigned i = 0; i < p_ai->mNumVertices; ++i )
                    mesh.m_uvs[ i ] = { p_ai->mTextureCoords[ 0 ][ i ].x, p_ai->mTextureCoords[ 0 ][ i ].y };
            }

            if ( p_ai->HasNormals() )
            {
                mesh.m_normals.resize( p_ai->mNumVertices );
                for ( unsigned i = 0; i < p_ai->mNumVertices; ++i )
                    mesh.m_normals[ i ] = { p_ai->mNormals[ i ].x, p_ai->mNormals[ i ].y, p_ai->mNormals[ i ].z };
            }

            if ( p_ai->HasTangentsAndBitangents() )
            {
                mesh.m_tangents.resize( p_ai->mNumVertices );
                for ( unsigned i = 0; i < p_ai->mNumVertices; ++i )
                {
                    const glm::vec3 t{ p_ai->mTangents[ i ].x, p_ai->mTangents[ i ].y, p_ai->mTangents[ i ].z };
                    const glm::vec3 b{ p_ai->mBitangents[ i ].x, p_ai->mBitangents[ i ].y, p_ai->mBitangents[ i ].z };
                    const glm::vec3 n{ p_ai->mNormals[ i ].x, p_ai->mNormals[ i ].y, p_ai->mNormals[ i ].z };
                    const float     w = ( glm::dot( glm::cross( n, t ), b ) >= 0.0f ) ? 1.0f : -1.0f;
                    mesh.m_tangents[ i ] = { t.x, t.y, t.z, w };
                }
            }
            else
            {
                mesh.m_tangents.assign( p_ai->mNumVertices, glm::vec4( 1.0f, 0.0f, 0.0f, 1.0f ) );
            }

            if ( p_ai->HasBones() && p_ai->mNumBones > 0 )
            {
                if ( p_ai->mNumBones > g_kMaxBonesPerSkin )
                {
                    TLOG_WARN() << "[TGltfLoader] Mesh '" << p_ai->mName.C_Str() << "' has " << p_ai->mNumBones << " bones (max " << g_kMaxBonesPerSkin
                                << ") — skinning skipped\n";
                }
                else
                {
                    mesh.m_skinned = true;
                    const unsigned          vCount = p_ai->mNumVertices;
                    std::vector<glm::uvec4> joints( vCount, glm::uvec4( 0 ) );
                    std::vector<glm::vec4>  weights( vCount, glm::vec4( 0.0f ) );
                    std::vector<uint8_t>    slots( vCount, 0 );

                    const unsigned boneCount = std::min( p_ai->mNumBones, static_cast<unsigned>( g_kMaxBonesPerSkin ) );
                    mesh.m_boneNames.reserve( boneCount );
                    mesh.m_inverseBindMatrices.reserve( boneCount );

                    for ( unsigned b = 0; b < boneCount; ++b )
                    {
                        const aiBone* bone = p_ai->mBones[ b ];
                        mesh.m_boneNames.emplace_back( bone->mName.C_Str() );
                        mesh.m_inverseBindMatrices.push_back( aiToGlm( bone->mOffsetMatrix ) );

                        for ( unsigned w = 0; w < bone->mNumWeights; ++w )
                        {
                            const unsigned vi = bone->mWeights[ w ].mVertexId;
                            const float    wt = bone->mWeights[ w ].mWeight;
                            if ( vi >= vCount || wt <= 0.0f ) continue;

                            uint8_t& slot = slots[ vi ];
                            if ( slot >= 4 ) continue;
                            joints[ vi ][ slot ]  = b;
                            weights[ vi ][ slot ] = wt;
                            ++slot;
                        }
                    }

                    for ( unsigned i = 0; i < vCount; ++i )
                    {
                        const float sum = weights[ i ].x + weights[ i ].y + weights[ i ].z + weights[ i ].w;
                        if ( sum > 1e-6f ) weights[ i ] /= sum;
                    }

                    mesh.m_joints  = std::move( joints );
                    mesh.m_weights = std::move( weights );
                }
            }

            if ( p_ai->HasFaces() )
            {
                const bool use16     = ( p_ai->mNumVertices <= 65535 );
                mesh.m_is16BitIndex = use16;

                if ( use16 )
                {
                    mesh.m_indices16.reserve( p_ai->mNumFaces * 3 );
                    for ( unsigned f = 0; f < p_ai->mNumFaces; ++f )
                        for ( unsigned j = 0; j < p_ai->mFaces[ f ].mNumIndices; ++j )
                            mesh.m_indices16.push_back( static_cast<uint16_t>( p_ai->mFaces[ f ].mIndices[ j ] ) );
                    mesh.m_drawCount = static_cast<uint32_t>( mesh.m_indices16.size() );
                }
                else
                {
                    mesh.m_indices32.reserve( p_ai->mNumFaces * 3 );
                    for ( unsigned f = 0; f < p_ai->mNumFaces; ++f )
                        for ( unsigned j = 0; j < p_ai->mFaces[ f ].mNumIndices; ++j ) mesh.m_indices32.push_back( p_ai->mFaces[ f ].mIndices[ j ] );
                    mesh.m_drawCount = static_cast<uint32_t>( mesh.m_indices32.size() );
                }
            }
            else
            {
                mesh.m_drawCount = p_ai->mNumVertices;
            }

            return mesh;
        }

        int32_t pushTexture( const aiScene* p_scene, const std::string& p_texPath, const std::string& p_baseDir, bool p_isSrgb,
                             std::vector<TCpuTextureData>& p_textures )
        {
            TCpuTextureData tex{};
            tex.m_isSrgb = p_isSrgb;
            if ( !decodeTexturePixels( p_scene, p_texPath, p_baseDir, tex ) ) return g_kTexMissing;
            const int32_t idx = static_cast<int32_t>( p_textures.size() );
            p_textures.push_back( std::move( tex ) );
            return idx;
        }

        TCpuMaterialData extractMaterial( const aiScene* p_scene, const aiMaterial* p_mat, const std::string& p_baseDir,
                                          std::vector<TCpuTextureData>& p_textures )
        {
            TCpuMaterialData desc{};

            aiColor4D col;
            if ( aiGetMaterialColor( p_mat, AI_MATKEY_BASE_COLOR, &col ) == AI_SUCCESS ) desc.m_baseColorFactor = { col.r, col.g, col.b, col.a };

            float metallic = 0.0f, roughness = 1.0f;
            aiGetMaterialFloat( p_mat, AI_MATKEY_METALLIC_FACTOR, &metallic );
            aiGetMaterialFloat( p_mat, AI_MATKEY_ROUGHNESS_FACTOR, &roughness );
            desc.m_metallicFactor  = metallic;
            desc.m_roughnessFactor = roughness;

            aiColor3D emissive{ 0.0f, 0.0f, 0.0f };
            aiGetMaterialColor( p_mat, AI_MATKEY_COLOR_EMISSIVE, reinterpret_cast<aiColor4D*>( &emissive ) );
            desc.m_emissionFactor = { emissive.r, emissive.g, emissive.b };

            aiString alphaMode;
            if ( p_mat->Get( AI_MATKEY_GLTF_ALPHAMODE, alphaMode ) == AI_SUCCESS )
            {
                const std::string mode = alphaMode.C_Str();
                if ( mode == "MASK" )
                    desc.m_alphaMode = TMatAlpha::Mask;
                else if ( mode == "BLEND" )
                    desc.m_alphaMode = TMatAlpha::Blend;
            }

            float alphaCutoff = 0.5f;
            aiGetMaterialFloat( p_mat, AI_MATKEY_GLTF_ALPHACUTOFF, &alphaCutoff );
            desc.m_alphaCutoff = alphaCutoff;

            int twoSided = 0;
            if ( p_mat->Get( AI_MATKEY_TWOSIDED, twoSided ) == AI_SUCCESS ) desc.m_doubleSided = twoSided != 0;

            auto resolve = [ & ]( aiTextureType p_type, bool p_srgb ) -> int32_t
            {
                aiString path;
                if ( p_mat->GetTexture( p_type, 0, &path ) != AI_SUCCESS ) return g_kTexDefault;
                const int32_t idx = pushTexture( p_scene, path.C_Str(), p_baseDir, p_srgb, p_textures );
                if ( idx == g_kTexMissing ) TLOG_WARN() << "[TGltfLoader] Failed to load texture '" << path.C_Str() << "' — using missing texture";
                return idx;
            };

            desc.m_baseTex     = resolve( aiTextureType_BASE_COLOR, true );
            desc.m_metRghTex   = resolve( aiTextureType_METALNESS, false );
            desc.m_emissionTex = resolve( aiTextureType_EMISSIVE, true );

            {
                aiString normPath;
                if ( p_mat->GetTexture( aiTextureType_NORMALS, 0, &normPath ) == AI_SUCCESS )
                {
                    desc.m_normalTex    = pushTexture( p_scene, normPath.C_Str(), p_baseDir, false, p_textures );
                    desc.m_hasNormalMap = true;
                    if ( desc.m_normalTex == g_kTexMissing )
                        TLOG_WARN() << "[TGltfLoader] Failed to load normal map '" << normPath.C_Str() << "' — using missing texture";
                }
                float normalScale = 1.0f;
                if ( aiGetMaterialFloat( p_mat, AI_MATKEY_GLTF_TEXTURE_SCALE( aiTextureType_NORMALS, 0 ), &normalScale ) == AI_SUCCESS )
                    desc.m_normalScale = normalScale;
                if ( !desc.m_hasNormalMap ) desc.m_normalTex = g_kTexDefault;
            }

            return desc;
        }

        TAnimationClip extractClip( const aiAnimation* p_anim )
        {
            TAnimationClip clip{};
            clip.m_name = p_anim->mName.C_Str();
            if ( clip.m_name.empty() ) clip.m_name = "clip";

            const double tps = ( p_anim->mTicksPerSecond > 0.0 ) ? p_anim->mTicksPerSecond : 25.0;
            clip.m_duration  = static_cast<float>( p_anim->mDuration / tps );

            for ( unsigned c = 0; c < p_anim->mNumChannels; ++c )
            {
                const aiNodeAnim* channel    = p_anim->mChannels[ c ];
                const std::string targetName = channel->mNodeName.C_Str();
                if ( targetName.empty() ) continue;

                auto makeChannel = [ & ]( TAnimPath p_path )
                {
                    TAnimationChannel ch{};
                    ch.m_targetName = targetName;
                    ch.m_path       = p_path;
                    return ch;
                };

                if ( channel->mNumPositionKeys > 0 )
                {
                    TAnimationChannel ch = makeChannel( TAnimPath::Translation );
                    ch.m_times.reserve( channel->mNumPositionKeys );
                    ch.m_translations.reserve( channel->mNumPositionKeys );
                    for ( unsigned k = 0; k < channel->mNumPositionKeys; ++k )
                    {
                        const aiVectorKey& key = channel->mPositionKeys[ k ];
                        ch.m_times.push_back( static_cast<float>( key.mTime / tps ) );
                        ch.m_translations.emplace_back( key.mValue.x, key.mValue.y, key.mValue.z );
                    }
                    clip.m_channels.push_back( std::move( ch ) );
                }

                if ( channel->mNumRotationKeys > 0 )
                {
                    TAnimationChannel ch = makeChannel( TAnimPath::Rotation );
                    ch.m_times.reserve( channel->mNumRotationKeys );
                    ch.m_rotations.reserve( channel->mNumRotationKeys );
                    for ( unsigned k = 0; k < channel->mNumRotationKeys; ++k )
                    {
                        const aiQuatKey& key = channel->mRotationKeys[ k ];
                        ch.m_times.push_back( static_cast<float>( key.mTime / tps ) );
                        ch.m_rotations.emplace_back( key.mValue.w, key.mValue.x, key.mValue.y, key.mValue.z );
                    }
                    clip.m_channels.push_back( std::move( ch ) );
                }

                if ( channel->mNumScalingKeys > 0 )
                {
                    TAnimationChannel ch = makeChannel( TAnimPath::Scale );
                    ch.m_times.reserve( channel->mNumScalingKeys );
                    ch.m_scales.reserve( channel->mNumScalingKeys );
                    for ( unsigned k = 0; k < channel->mNumScalingKeys; ++k )
                    {
                        const aiVectorKey& key = channel->mScalingKeys[ k ];
                        ch.m_times.push_back( static_cast<float>( key.mTime / tps ) );
                        ch.m_scales.emplace_back( key.mValue.x, key.mValue.y, key.mValue.z );
                    }
                    clip.m_channels.push_back( std::move( ch ) );
                }
            }

            return clip;
        }

        std::unique_ptr<TVkMesh> uploadMesh( const TCpuMeshData& p_cpu, TVkGpu& p_gpu )
        {
            auto mesh = std::make_unique<TVkMesh>();
            mesh->m_aabb         = p_cpu.m_aabb;
            mesh->m_drawCount    = p_cpu.m_drawCount;
            mesh->m_is16BitIndex = p_cpu.m_is16BitIndex;

            {
                const size_t sz  = p_cpu.m_positions.size() * sizeof( glm::vec3 );
                mesh->m_position = std::make_unique<TVkBuffer>( p_gpu.device(), p_gpu.physDevice(), sz, TBufUsage::Vertex | TBufUsage::CopyDst );
                p_gpu.uploadBuffer( *mesh->m_position, p_cpu.m_positions.data(), sz );
            }

            if ( !p_cpu.m_uvs.empty() )
            {
                const size_t sz  = p_cpu.m_uvs.size() * sizeof( glm::vec2 );
                mesh->m_texCoord = std::make_unique<TVkBuffer>( p_gpu.device(), p_gpu.physDevice(), sz, TBufUsage::Vertex | TBufUsage::CopyDst );
                p_gpu.uploadBuffer( *mesh->m_texCoord, p_cpu.m_uvs.data(), sz );
            }

            if ( !p_cpu.m_normals.empty() )
            {
                const size_t sz = p_cpu.m_normals.size() * sizeof( glm::vec3 );
                mesh->m_normal  = std::make_unique<TVkBuffer>( p_gpu.device(), p_gpu.physDevice(), sz, TBufUsage::Vertex | TBufUsage::CopyDst );
                p_gpu.uploadBuffer( *mesh->m_normal, p_cpu.m_normals.data(), sz );
            }

            if ( !p_cpu.m_tangents.empty() )
            {
                const size_t sz = p_cpu.m_tangents.size() * sizeof( glm::vec4 );
                mesh->m_tangent = std::make_unique<TVkBuffer>( p_gpu.device(), p_gpu.physDevice(), sz, TBufUsage::Vertex | TBufUsage::CopyDst );
                p_gpu.uploadBuffer( *mesh->m_tangent, p_cpu.m_tangents.data(), sz );
            }

            if ( !p_cpu.m_joints.empty() && !p_cpu.m_weights.empty() )
            {
                const size_t jSz = p_cpu.m_joints.size() * sizeof( glm::uvec4 );
                const size_t wSz = p_cpu.m_weights.size() * sizeof( glm::vec4 );
                mesh->m_joints   = std::make_unique<TVkBuffer>( p_gpu.device(), p_gpu.physDevice(), jSz, TBufUsage::Vertex | TBufUsage::CopyDst );
                mesh->m_weights  = std::make_unique<TVkBuffer>( p_gpu.device(), p_gpu.physDevice(), wSz, TBufUsage::Vertex | TBufUsage::CopyDst );
                p_gpu.uploadBuffer( *mesh->m_joints, p_cpu.m_joints.data(), jSz );
                p_gpu.uploadBuffer( *mesh->m_weights, p_cpu.m_weights.data(), wSz );
            }

            if ( !p_cpu.m_indices16.empty() )
            {
                const size_t sz = p_cpu.m_indices16.size() * sizeof( uint16_t );
                mesh->m_index   = std::make_unique<TVkBuffer>( p_gpu.device(), p_gpu.physDevice(), sz, TBufUsage::Index | TBufUsage::CopyDst );
                p_gpu.uploadBuffer( *mesh->m_index, p_cpu.m_indices16.data(), sz );
            }
            else if ( !p_cpu.m_indices32.empty() )
            {
                const size_t sz = p_cpu.m_indices32.size() * sizeof( uint32_t );
                mesh->m_index   = std::make_unique<TVkBuffer>( p_gpu.device(), p_gpu.physDevice(), sz, TBufUsage::Index | TBufUsage::CopyDst );
                p_gpu.uploadBuffer( *mesh->m_index, p_cpu.m_indices32.data(), sz );
            }

            return mesh;
        }

        const TVkImage* resolveTex( int32_t p_idx, const std::vector<std::unique_ptr<TVkImage>>& p_images, TVkGpu& p_gpu )
        {
            if ( p_idx == g_kTexMissing ) return &p_gpu.missingTexture();
            if ( p_idx < 0 ) return &p_gpu.defaultTexture();
            const auto u = static_cast<size_t>( p_idx );
            if ( u >= p_images.size() || p_images[ u ] == nullptr ) return &p_gpu.missingTexture();
            return p_images[ u ].get();
        }
    }  // namespace

    TCpuGltfPackage TGltfLoader::decodeCpu( const std::string& p_path )
    {
        TCpuGltfPackage pkg{};
        pkg.m_sourcePath = p_path;
        pkg.m_id         = TAssetSystem::makeStableId( p_path );

        const std::string filePath = TPath::resolveString( p_path );

        Assimp::Importer importer;
        const aiScene*   scene = importer.ReadFile( filePath, aiProcess_Triangulate | aiProcess_GenSmoothNormals | aiProcess_CalcTangentSpace |
                                                                      aiProcess_JoinIdenticalVertices | aiProcess_LimitBoneWeights | aiProcess_FlipUVs );

        if ( scene == nullptr || ( scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE ) || scene->mRootNode == nullptr )
        {
            pkg.m_error = importer.GetErrorString();
            pkg.m_ok    = false;
            return pkg;
        }

        const std::string baseDir = [ & ]
        {
            const size_t sep = filePath.find_last_of( "/\\" );
            return ( sep == std::string::npos ) ? std::string{ "." } : filePath.substr( 0, sep );
        }();

        pkg.m_name = [ & ]
        {
            const size_t      sep  = p_path.find_last_of( "/\\" );
            const std::string name = ( sep == std::string::npos ) ? p_path : p_path.substr( sep + 1 );
            const size_t      dot  = name.find_last_of( '.' );
            return ( dot == std::string::npos ) ? name : name.substr( 0, dot );
        }();

        pkg.m_meshes.reserve( scene->mNumMeshes );
        for ( unsigned i = 0; i < scene->mNumMeshes; ++i ) pkg.m_meshes.push_back( extractMesh( scene->mMeshes[ i ] ) );

        pkg.m_materials.reserve( scene->mNumMaterials );
        for ( unsigned i = 0; i < scene->mNumMaterials; ++i )
            pkg.m_materials.push_back( extractMaterial( scene, scene->mMaterials[ i ], baseDir, pkg.m_textures ) );

        struct StackEntry
        {
            aiNode*     m_aiNode;
            int32_t     m_parent;
            std::string m_path;
        };

        std::vector<StackEntry> stack;
        stack.push_back( { scene->mRootNode, -1, scene->mRootNode->mName.C_Str() } );

        while ( !stack.empty() )
        {
            auto [ node, parent, path ] = stack.back();
            stack.pop_back();

            TCpuNodeData n{};
            n.m_name   = node->mName.C_Str();
            n.m_parent = parent;

            {
                aiVector3D   aiScale, aiPos;
                aiQuaternion aiRot;
                node->mTransformation.Decompose( aiScale, aiRot, aiPos );
                n.m_translation = { aiPos.x, aiPos.y, aiPos.z };
                n.m_rotation    = glm::quat{ aiRot.w, aiRot.x, aiRot.y, aiRot.z };
                n.m_scale       = { aiScale.x, aiScale.y, aiScale.z };
            }

            n.m_meshIndices.reserve( node->mNumMeshes );
            for ( unsigned i = 0; i < node->mNumMeshes; ++i ) n.m_meshIndices.push_back( node->mMeshes[ i ] );

            const int32_t selfIdx = static_cast<int32_t>( pkg.m_nodes.size() );
            pkg.m_nodes.push_back( std::move( n ) );

            for ( unsigned i = node->mNumChildren; i > 0; --i )
            {
                aiNode*           childAi   = node->mChildren[ i - 1 ];
                const std::string childPath = path + "/" + childAi->mName.C_Str();
                stack.push_back( { childAi, selfIdx, childPath } );
            }
        }

        pkg.m_clips.reserve( scene->mNumAnimations );
        for ( unsigned i = 0; i < scene->mNumAnimations; ++i )
        {
            TAnimationClip clip = extractClip( scene->mAnimations[ i ] );
            if ( !clip.m_channels.empty() ) pkg.m_clips.push_back( std::move( clip ) );
        }

        if ( !pkg.m_clips.empty() ) TLOG_INFO() << "[TGltfLoader] Decoded " << pkg.m_clips.size() << " animation clip(s) for '" << pkg.m_name << "'\n";

        pkg.m_ok = true;
        return pkg;
    }

    TLoadResult TGltfLoader::uploadGpu( TCpuGltfPackage&& p_package, TVkGpu& p_gpu, TLevelStore& p_store )
    {
        if ( !p_package.m_ok )
            throw std::runtime_error( "[TGltfLoader] Failed to load '" + p_package.m_sourcePath + "': " + p_package.m_error );

        auto asset          = std::make_unique<TGpuAsset>();
        asset->m_name       = p_package.m_name;
        asset->m_sourcePath = p_package.m_sourcePath;
        asset->m_id         = p_package.m_id.empty() ? TAssetSystem::makeStableId( p_package.m_sourcePath ) : p_package.m_id;

        p_gpu.beginUploadBatch();

        asset->m_textures.reserve( p_package.m_textures.size() );
        for ( const TCpuTextureData& tex : p_package.m_textures )
        {
            TVkImageDesc desc{};
            desc.m_width     = tex.m_width;
            desc.m_height    = tex.m_height;
            desc.m_mipLevels = TVkImage::calcMipLevels( desc.m_width, desc.m_height );
            desc.m_format    = tex.m_isSrgb ? TImgFormat::RGBA8Srgb : TImgFormat::RGBA8Unorm;
            desc.m_usage     = TImgUsage::CopySrc | TImgUsage::CopyDst | TImgUsage::Sampled;
            desc.m_sampled   = true;
            desc.m_addr      = TTexAddr::Repeat;

            auto image = std::make_unique<TVkImage>( p_gpu.device(), p_gpu.physDevice(), desc );
            p_gpu.uploadImage( *image, tex.m_pixels.data(), desc.m_width, desc.m_height );
            asset->m_textures.push_back( std::move( image ) );
        }

        asset->m_meshes.reserve( p_package.m_meshes.size() );
        for ( const TCpuMeshData& mesh : p_package.m_meshes ) asset->m_meshes.push_back( uploadMesh( mesh, p_gpu ) );

        p_gpu.endUploadBatch();

        asset->m_materials.reserve( p_package.m_materials.size() );
        for ( const TCpuMaterialData& mat : p_package.m_materials )
        {
            TVkMaterialDesc desc{};
            desc.m_baseColorFactor = mat.m_baseColorFactor;
            desc.m_emissionFactor  = mat.m_emissionFactor;
            desc.m_metallicFactor  = mat.m_metallicFactor;
            desc.m_roughnessFactor = mat.m_roughnessFactor;
            desc.m_normalScale     = mat.m_normalScale;
            desc.m_alphaCutoff     = mat.m_alphaCutoff;
            desc.m_alphaMode       = mat.m_alphaMode;
            desc.m_doubleSided     = mat.m_doubleSided;
            desc.m_hasNormalMap    = mat.m_hasNormalMap;
            desc.m_baseTexture     = resolveTex( mat.m_baseTex, asset->m_textures, p_gpu );
            desc.m_metRghTexture   = resolveTex( mat.m_metRghTex, asset->m_textures, p_gpu );
            desc.m_emissionTexture = resolveTex( mat.m_emissionTex, asset->m_textures, p_gpu );
            desc.m_normalTexture   = resolveTex( mat.m_normalTex, asset->m_textures, p_gpu );

            asset->m_materials.push_back(
                    std::make_unique<TVkMaterial>( p_gpu.device(), p_gpu.physDevice(), p_gpu.descPool(), p_gpu.layouts().m_material, desc ) );
        }

        asset->m_clips.reserve( p_package.m_clips.size() );
        for ( TAnimationClip& clip : p_package.m_clips ) asset->m_clips.push_back( std::make_unique<TAnimationClip>( std::move( clip ) ) );

        if ( p_package.m_nodes.empty() ) return { nullptr, std::move( asset ) };

        std::vector<TSceneNode*>                 nodes;
        nodes.reserve( p_package.m_nodes.size() );
        std::unordered_map<std::string, TSceneNode*> byName;

        for ( size_t i = 0; i < p_package.m_nodes.size(); ++i )
        {
            const TCpuNodeData& nd = p_package.m_nodes[ i ];
            TNodeHandle         h  = p_store.createNode( nd.m_name );
            TSceneNode*         n  = p_store.getNode( h );
            n->m_transform.setLocalTRS( nd.m_translation, nd.m_rotation, nd.m_scale );
            nodes.push_back( n );
            if ( !byName.emplace( nd.m_name, n ).second )
                TLOG_WARN() << "[TGltfLoader] Duplicate Assimp node name '" << nd.m_name << "' — skin binds keep the first occurrence\n";
        }

        for ( size_t i = 0; i < p_package.m_nodes.size(); ++i )
        {
            const TCpuNodeData& nd = p_package.m_nodes[ i ];
            if ( nd.m_parent >= 0 && static_cast<size_t>( nd.m_parent ) < nodes.size() )
                nodes[ static_cast<size_t>( nd.m_parent ) ]->addChild( nodes[ i ] );
        }

        const std::string& stem = asset->m_name;
        for ( size_t i = 0; i < p_package.m_nodes.size(); ++i )
        {
            const TCpuNodeData& nd    = p_package.m_nodes[ i ];
            TSceneNode*         tNode = nodes[ i ];
            for ( uint32_t meshIdx : nd.m_meshIndices )
            {
                if ( meshIdx >= asset->m_meshes.size() ) continue;
                const TCpuMeshData& cpuMesh = p_package.m_meshes[ meshIdx ];
                const unsigned      matIdx  = cpuMesh.m_materialIndex;
                TVkMesh*            mesh    = asset->m_meshes[ meshIdx ].get();
                TVkMaterial*        mat     = ( matIdx < asset->m_materials.size() ) ? asset->m_materials[ matIdx ].get() : nullptr;

                TMeshAssetRef ref{ stem, meshIdx, matIdx };
                if ( cpuMesh.m_skinned && mesh->isSkinned() )
                {
                    std::vector<TSkinJoint> joints;
                    joints.reserve( cpuMesh.m_boneNames.size() );
                    for ( size_t b = 0; b < cpuMesh.m_boneNames.size(); ++b )
                    {
                        TSkinJoint j{};
                        j.m_inverseBindMtx = cpuMesh.m_inverseBindMatrices[ b ];
                        const auto it      = byName.find( cpuMesh.m_boneNames[ b ] );
                        if ( it != byName.end() )
                            j.bind( it->second );
                        else
                            TLOG_WARN() << "[TGltfLoader] Bone node not found: " << cpuMesh.m_boneNames[ b ] << "\n";
                        joints.push_back( std::move( j ) );
                    }
                    tNode->emplaceComponent<TSkinnedMeshComponent>( ref, mesh, mat, 0, std::move( joints ) );
                }
                else
                {
                    tNode->emplaceComponent<TMeshComponent>( ref, mesh, mat, 0 );
                }
            }
        }

        return { nodes.front(), std::move( asset ) };
    }

    TLoadResult TGltfLoader::load( const std::string& p_path, TVkGpu& p_gpu, TLevelStore& p_store )
    {
        return uploadGpu( decodeCpu( p_path ), p_gpu, p_store );
    }
}  // namespace Tomos
