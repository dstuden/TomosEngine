#include "Tomos/systems/asset/TGltfLoader.hh"

#include <algorithm>
#include <array>
#include <assimp/GltfMaterial.h>
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <cmath>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>
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
#include "Tomos/util/path/TPath.hh"

namespace Tomos
{
    static std::unique_ptr<TVkImage> loadTexture( const aiScene* p_scene, const std::string& p_texPath, const std::string& p_baseDir, TVkGpu& p_gpu,
                                                  bool p_isSrgb )
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

        if ( pixels == nullptr ) return nullptr;

        TVkImageDesc desc{};
        desc.m_width     = static_cast<uint32_t>( w );
        desc.m_height    = static_cast<uint32_t>( h );
        desc.m_mipLevels = TVkImage::calcMipLevels( desc.m_width, desc.m_height );
        desc.m_format    = p_isSrgb ? TImgFormat::RGBA8Srgb : TImgFormat::RGBA8Unorm;
        // CopySrc is required so uploadImage can blit the mip chain.
        desc.m_usage   = TImgUsage::CopySrc | TImgUsage::CopyDst | TImgUsage::Sampled;
        desc.m_sampled = true;
        desc.m_addr    = TTexAddr::Repeat;

        auto image = std::make_unique<TVkImage>( p_gpu.device(), p_gpu.physDevice(), desc );
        p_gpu.uploadImage( *image, pixels, desc.m_width, desc.m_height );

        if ( stbOwned )
            stbi_image_free( pixels );
        else
            delete[] pixels;

        return image;
    }

    static glm::mat4 aiToGlm( const aiMatrix4x4& p_m )
    {
        // Assimp row-major → glm column-major.
        return glm::mat4( p_m.a1, p_m.b1, p_m.c1, p_m.d1, p_m.a2, p_m.b2, p_m.c2, p_m.d2, p_m.a3, p_m.b3, p_m.c3, p_m.d3, p_m.a4, p_m.b4, p_m.c4, p_m.d4 );
    }

    static std::unique_ptr<TVkMesh> buildMesh( const aiMesh* p_ai, TVkGpu& p_gpu )
    {
        auto mesh = std::make_unique<TVkMesh>();

        {
            std::vector<glm::vec3> pos( p_ai->mNumVertices );
            glm::vec3              bmin( std::numeric_limits<float>::max() );
            glm::vec3              bmax( std::numeric_limits<float>::lowest() );
            for ( unsigned i = 0; i < p_ai->mNumVertices; ++i )
            {
                pos[ i ] = { p_ai->mVertices[ i ].x, p_ai->mVertices[ i ].y, p_ai->mVertices[ i ].z };
                bmin     = glm::min( bmin, pos[ i ] );
                bmax     = glm::max( bmax, pos[ i ] );
            }
            mesh->m_aabb.m_min = bmin;
            mesh->m_aabb.m_max = bmax;

            const size_t sz  = pos.size() * sizeof( glm::vec3 );
            mesh->m_position = std::make_unique<TVkBuffer>( p_gpu.device(), p_gpu.physDevice(), sz, TBufUsage::Vertex | TBufUsage::CopyDst );
            p_gpu.uploadBuffer( *mesh->m_position, pos.data(), sz );
        }

        if ( p_ai->HasTextureCoords( 0 ) )
        {
            std::vector<glm::vec2> uv( p_ai->mNumVertices );
            for ( unsigned i = 0; i < p_ai->mNumVertices; ++i ) uv[ i ] = { p_ai->mTextureCoords[ 0 ][ i ].x, p_ai->mTextureCoords[ 0 ][ i ].y };
            const size_t sz  = uv.size() * sizeof( glm::vec2 );
            mesh->m_texCoord = std::make_unique<TVkBuffer>( p_gpu.device(), p_gpu.physDevice(), sz, TBufUsage::Vertex | TBufUsage::CopyDst );
            p_gpu.uploadBuffer( *mesh->m_texCoord, uv.data(), sz );
        }

        if ( p_ai->HasNormals() )
        {
            std::vector<glm::vec3> nor( p_ai->mNumVertices );
            for ( unsigned i = 0; i < p_ai->mNumVertices; ++i ) nor[ i ] = { p_ai->mNormals[ i ].x, p_ai->mNormals[ i ].y, p_ai->mNormals[ i ].z };
            const size_t sz = nor.size() * sizeof( glm::vec3 );
            mesh->m_normal  = std::make_unique<TVkBuffer>( p_gpu.device(), p_gpu.physDevice(), sz, TBufUsage::Vertex | TBufUsage::CopyDst );
            p_gpu.uploadBuffer( *mesh->m_normal, nor.data(), sz );
        }

        if ( p_ai->HasTangentsAndBitangents() )
        {
            std::vector<glm::vec4> tan( p_ai->mNumVertices );
            for ( unsigned i = 0; i < p_ai->mNumVertices; ++i )
            {
                const glm::vec3 T{ p_ai->mTangents[ i ].x, p_ai->mTangents[ i ].y, p_ai->mTangents[ i ].z };
                const glm::vec3 B{ p_ai->mBitangents[ i ].x, p_ai->mBitangents[ i ].y, p_ai->mBitangents[ i ].z };
                const glm::vec3 N{ p_ai->mNormals[ i ].x, p_ai->mNormals[ i ].y, p_ai->mNormals[ i ].z };
                const float     w = ( glm::dot( glm::cross( N, T ), B ) >= 0.0f ) ? 1.0f : -1.0f;
                tan[ i ]          = { T.x, T.y, T.z, w };
            }
            const size_t sz = tan.size() * sizeof( glm::vec4 );
            mesh->m_tangent = std::make_unique<TVkBuffer>( p_gpu.device(), p_gpu.physDevice(), sz, TBufUsage::Vertex | TBufUsage::CopyDst );
            p_gpu.uploadBuffer( *mesh->m_tangent, tan.data(), sz );
        }
        else
        {
            // Flat fallback so pipelines can always bind a tangent stream.
            std::vector<glm::vec4> tan( p_ai->mNumVertices, glm::vec4( 1.0f, 0.0f, 0.0f, 1.0f ) );
            const size_t           sz = tan.size() * sizeof( glm::vec4 );
            mesh->m_tangent           = std::make_unique<TVkBuffer>( p_gpu.device(), p_gpu.physDevice(), sz, TBufUsage::Vertex | TBufUsage::CopyDst );
            p_gpu.uploadBuffer( *mesh->m_tangent, tan.data(), sz );
        }

        if ( p_ai->HasBones() && p_ai->mNumBones > 0 )
        {
            if ( p_ai->mNumBones > k_maxBonesPerSkin )
            {
                TLOG_WARN() << "[TGltfLoader] Mesh '" << p_ai->mName.C_Str() << "' has " << p_ai->mNumBones << " bones (max " << k_maxBonesPerSkin
                            << ") — skinning skipped\n";
            }
            else
            {
                const unsigned          vCount = p_ai->mNumVertices;
                std::vector<glm::uvec4> joints( vCount, glm::uvec4( 0 ) );
                std::vector<glm::vec4>  weights( vCount, glm::vec4( 0.0f ) );
                std::vector<uint8_t>    slots( vCount, 0 );

                const unsigned boneCount = std::min( p_ai->mNumBones, static_cast<unsigned>( k_maxBonesPerSkin ) );
                for ( unsigned b = 0; b < boneCount; ++b )
                {
                    const aiBone* bone = p_ai->mBones[ b ];
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

                const size_t jSz = joints.size() * sizeof( glm::uvec4 );
                const size_t wSz = weights.size() * sizeof( glm::vec4 );
                mesh->m_joints   = std::make_unique<TVkBuffer>( p_gpu.device(), p_gpu.physDevice(), jSz, TBufUsage::Vertex | TBufUsage::CopyDst );
                mesh->m_weights  = std::make_unique<TVkBuffer>( p_gpu.device(), p_gpu.physDevice(), wSz, TBufUsage::Vertex | TBufUsage::CopyDst );
                p_gpu.uploadBuffer( *mesh->m_joints, joints.data(), jSz );
                p_gpu.uploadBuffer( *mesh->m_weights, weights.data(), wSz );
            }
        }

        if ( p_ai->HasFaces() )
        {
            const bool use16     = ( p_ai->mNumVertices <= 65535 );
            mesh->m_is16BitIndex = use16;

            if ( use16 )
            {
                std::vector<uint16_t> idx;
                idx.reserve( p_ai->mNumFaces * 3 );
                for ( unsigned f = 0; f < p_ai->mNumFaces; ++f )
                    for ( unsigned j = 0; j < p_ai->mFaces[ f ].mNumIndices; ++j ) idx.push_back( static_cast<uint16_t>( p_ai->mFaces[ f ].mIndices[ j ] ) );
                mesh->m_drawCount = static_cast<uint32_t>( idx.size() );
                const size_t sz   = idx.size() * sizeof( uint16_t );
                mesh->m_index     = std::make_unique<TVkBuffer>( p_gpu.device(), p_gpu.physDevice(), sz, TBufUsage::Index | TBufUsage::CopyDst );
                p_gpu.uploadBuffer( *mesh->m_index, idx.data(), sz );
            }
            else
            {
                std::vector<uint32_t> idx;
                idx.reserve( p_ai->mNumFaces * 3 );
                for ( unsigned f = 0; f < p_ai->mNumFaces; ++f )
                    for ( unsigned j = 0; j < p_ai->mFaces[ f ].mNumIndices; ++j ) idx.push_back( p_ai->mFaces[ f ].mIndices[ j ] );
                mesh->m_drawCount = static_cast<uint32_t>( idx.size() );
                const size_t sz   = idx.size() * sizeof( uint32_t );
                mesh->m_index     = std::make_unique<TVkBuffer>( p_gpu.device(), p_gpu.physDevice(), sz, TBufUsage::Index | TBufUsage::CopyDst );
                p_gpu.uploadBuffer( *mesh->m_index, idx.data(), sz );
            }
        }
        else
        {
            mesh->m_drawCount = p_ai->mNumVertices;
        }

        return mesh;
    }

    static std::unique_ptr<TVkMaterial> buildMaterial( const aiScene* p_scene, const aiMaterial* p_mat, const std::string& p_baseDir, TVkGpu& p_gpu,
                                                       std::vector<std::unique_ptr<TVkImage>>& p_textures )
    {
        auto resolveTexture = [ & ]( aiTextureType p_type, bool p_srgb ) -> const TVkImage*
        {
            aiString path;
            if ( p_mat->GetTexture( p_type, 0, &path ) != AI_SUCCESS ) return &p_gpu.defaultTexture();

            auto img = loadTexture( p_scene, path.C_Str(), p_baseDir, p_gpu, p_srgb );
            if ( !img )
            {
                TLOG_WARN() << "[TGltfLoader] Failed to load texture '" << path.C_Str() << "' — using missing texture";
                return &p_gpu.missingTexture();
            }

            const TVkImage* ptr = img.get();
            p_textures.push_back( std::move( img ) );
            return ptr;
        };

        TVkMaterialDesc desc{};

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

        desc.m_baseTexture     = resolveTexture( aiTextureType_BASE_COLOR, true );
        desc.m_metRghTexture   = resolveTexture( aiTextureType_METALNESS, false );
        desc.m_emissionTexture = resolveTexture( aiTextureType_EMISSIVE, true );
        {
            aiString normPath;
            if ( p_mat->GetTexture( aiTextureType_NORMALS, 0, &normPath ) == AI_SUCCESS )
            {
                auto img = loadTexture( p_scene, normPath.C_Str(), p_baseDir, p_gpu, false );
                if ( img )
                {
                    desc.m_normalTexture = img.get();
                    desc.m_hasNormalMap  = true;
                    p_textures.push_back( std::move( img ) );
                }
                else
                {
                    TLOG_WARN() << "[TGltfLoader] Failed to load normal map '" << normPath.C_Str() << "' — using missing texture";
                    desc.m_normalTexture = &p_gpu.missingTexture();
                    desc.m_hasNormalMap  = true;
                }
            }
            float normalScale = 1.0f;
            if ( aiGetMaterialFloat( p_mat, AI_MATKEY_GLTF_TEXTURE_SCALE( aiTextureType_NORMALS, 0 ), &normalScale ) == AI_SUCCESS )
                desc.m_normalScale = normalScale;
            if ( desc.m_normalTexture == nullptr ) desc.m_normalTexture = &p_gpu.defaultTexture();
        }

        return std::make_unique<TVkMaterial>( p_gpu.device(), p_gpu.physDevice(), p_gpu.descPool(), p_gpu.layouts().m_material, desc );
    }

    static std::vector<TSkinJoint> buildSkinJoints( const aiMesh* p_ai, const std::unordered_map<std::string, std::shared_ptr<TSceneNode>>& p_byName )
    {
        std::vector<TSkinJoint> joints;
        if ( !p_ai->HasBones() ) return joints;

        const unsigned boneCount = std::min( p_ai->mNumBones, static_cast<unsigned>( k_maxBonesPerSkin ) );
        joints.reserve( boneCount );

        for ( unsigned b = 0; b < boneCount; ++b )
        {
            const aiBone* bone = p_ai->mBones[ b ];
            TSkinJoint    j{};
            j.m_inverseBindMtx = aiToGlm( bone->mOffsetMatrix );

            const auto it = p_byName.find( bone->mName.C_Str() );
            if ( it != p_byName.end() )
                j.m_node = it->second;
            else
                TLOG_WARN() << "[TGltfLoader] Bone node not found: " << bone->mName.C_Str() << "\n";

            joints.push_back( std::move( j ) );
        }
        return joints;
    }

    static std::unique_ptr<TAnimationClip> buildClip( const aiAnimation* p_anim )
    {
        auto clip    = std::make_unique<TAnimationClip>();
        clip->m_name = p_anim->mName.C_Str();
        if ( clip->m_name.empty() ) clip->m_name = "clip";

        // Assimp duration is in ticks; convert to seconds.
        const double tps = ( p_anim->mTicksPerSecond > 0.0 ) ? p_anim->mTicksPerSecond : 25.0;
        clip->m_duration = static_cast<float>( p_anim->mDuration / tps );

        for ( unsigned c = 0; c < p_anim->mNumChannels; ++c )
        {
            const aiNodeAnim* channel    = p_anim->mChannels[ c ];
            const std::string targetName = channel->mNodeName.C_Str();
            if ( targetName.empty() ) continue;

            auto makeChannel = [ & ]( TAnimPath path )
            {
                TAnimationChannel ch{};
                ch.m_targetName = targetName;
                ch.m_path       = path;
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
                    ch.m_translations.push_back( { key.mValue.x, key.mValue.y, key.mValue.z } );
                }
                clip->m_channels.push_back( std::move( ch ) );
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
                    ch.m_rotations.push_back( glm::quat{ key.mValue.w, key.mValue.x, key.mValue.y, key.mValue.z } );
                }
                clip->m_channels.push_back( std::move( ch ) );
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
                    ch.m_scales.push_back( { key.mValue.x, key.mValue.y, key.mValue.z } );
                }
                clip->m_channels.push_back( std::move( ch ) );
            }
        }

        return clip;
    }

    TLoadResult TGltfLoader::load( const std::string& p_path, TVkGpu& p_gpu )
    {
        const std::string filePath = TPath::resolveString( p_path );

        Assimp::Importer importer;
        const aiScene*   scene = importer.ReadFile( filePath, aiProcess_Triangulate | aiProcess_GenSmoothNormals | aiProcess_CalcTangentSpace |
                                                                    aiProcess_JoinIdenticalVertices | aiProcess_LimitBoneWeights | aiProcess_FlipUVs );

        if ( scene == nullptr || ( scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE ) || scene->mRootNode == nullptr )
            throw std::runtime_error( "[TGltfLoader] Failed to load '" + p_path + "': " + importer.GetErrorString() );

        const std::string baseDir = [ & ]
        {
            const size_t sep = filePath.find_last_of( "/\\" );
            return ( sep == std::string::npos ) ? std::string{ "." } : filePath.substr( 0, sep );
        }();

        const std::string stem = [ & ]
        {
            const size_t      sep  = p_path.find_last_of( "/\\" );
            const std::string name = ( sep == std::string::npos ) ? p_path : p_path.substr( sep + 1 );
            const size_t      dot  = name.find_last_of( '.' );
            return ( dot == std::string::npos ) ? name : name.substr( 0, dot );
        }();

        auto asset          = std::make_unique<TGpuAsset>();
        asset->m_name       = stem;
        asset->m_sourcePath = p_path;
        asset->m_id         = TAssetSystem::makeStableId( p_path );

        asset->m_meshes.reserve( scene->mNumMeshes );
        for ( unsigned i = 0; i < scene->mNumMeshes; ++i ) asset->m_meshes.push_back( buildMesh( scene->mMeshes[ i ], p_gpu ) );

        asset->m_materials.reserve( scene->mNumMaterials );
        for ( unsigned i = 0; i < scene->mNumMaterials; ++i )
            asset->m_materials.push_back( buildMaterial( scene, scene->mMaterials[ i ], baseDir, p_gpu, asset->m_textures ) );

        auto root = std::make_shared<TSceneNode>( scene->mRootNode->mName.C_Str() );

        struct StackEntry
        {
            aiNode*                     m_aiNode;
            std::shared_ptr<TSceneNode> m_tNode;
            std::string                 m_path;
        };

        std::vector<StackEntry>                                      stack;
        std::vector<StackEntry>                                      allNodes;
        std::unordered_map<std::string, std::shared_ptr<TSceneNode>> byName;

        stack.reserve( 64 );
        stack.push_back( { scene->mRootNode, root, root->m_name } );
        byName.emplace( root->m_name, root );

        while ( !stack.empty() )
        {
            auto [ node, tNode, path ] = stack.back();
            stack.pop_back();
            allNodes.push_back( { node, tNode, path } );

            {
                aiVector3D   aiScale, aiPos;
                aiQuaternion aiRot;
                node->mTransformation.Decompose( aiScale, aiRot, aiPos );

                tNode->m_transform.setLocalTRS( { aiPos.x, aiPos.y, aiPos.z }, glm::quat{ aiRot.w, aiRot.x, aiRot.y, aiRot.z },
                                                { aiScale.x, aiScale.y, aiScale.z } );
            }

            for ( unsigned i = node->mNumChildren; i > 0; --i )
            {
                aiNode*           childAi   = node->mChildren[ i - 1 ];
                const auto        child     = std::make_shared<TSceneNode>( childAi->mName.C_Str() );
                const std::string childPath = path + "/" + child->m_name;
                tNode->addChild( child );
                if ( !byName.emplace( child->m_name, child ).second )
                    TLOG_WARN() << "[TGltfLoader] Duplicate Assimp node name '" << child->m_name << "' (path " << childPath
                                << ") — skin binds keep the first occurrence\n";
                stack.push_back( { childAi, child, childPath } );
            }
        }

        for ( const auto& [ node, tNode, path ] : allNodes )
        {
            ( void ) path;
            for ( unsigned i = 0; i < node->mNumMeshes; ++i )
            {
                const unsigned meshIdx = node->mMeshes[ i ];
                const unsigned matIdx  = scene->mMeshes[ meshIdx ]->mMaterialIndex;
                const aiMesh*  aiMesh  = scene->mMeshes[ meshIdx ];
                TVkMesh*       mesh    = asset->m_meshes[ meshIdx ].get();
                TVkMaterial*   mat     = asset->m_materials[ matIdx ].get();

                TMeshAssetRef ref{ stem, meshIdx, matIdx };
                if ( mesh->isSkinned() && aiMesh->HasBones() )
                {
                    auto joints = buildSkinJoints( aiMesh, byName );
                    tNode->addComponent( std::make_shared<TSkinnedMeshComponent>( ref, mesh, mat, 0, std::move( joints ) ) );
                }
                else
                {
                    tNode->addComponent( std::make_shared<TMeshComponent>( ref, mesh, mat, 0 ) );
                }
            }
        }

        asset->m_clips.reserve( scene->mNumAnimations );
        for ( unsigned i = 0; i < scene->mNumAnimations; ++i )
        {
            auto clip = buildClip( scene->mAnimations[ i ] );
            if ( !clip->m_channels.empty() ) asset->m_clips.push_back( std::move( clip ) );
        }

        if ( !asset->m_clips.empty() ) TLOG_INFO() << "[TGltfLoader] Loaded " << asset->m_clips.size() << " animation clip(s) for '" << stem << "'\n";

        return { std::move( root ), std::move( asset ) };
    }
}  // namespace Tomos
