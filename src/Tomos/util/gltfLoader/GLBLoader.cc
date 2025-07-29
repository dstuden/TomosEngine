#define GLM_ENABLE_EXPERIMENTAL

#include "GLBLoader.hh"

#include <filesystem>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/matrix_decompose.hpp>

#include "Tomos/systems/mesh/MeshComponent.hh"
#include "Tomos/util/logger/Logger.hh"
#include "Tomos/util/renderer/Buffer.hh"

namespace Tomos
{
    GLBLoader::LoadResult GLBLoader::loadGLB( const std::string& p_filepath, const std::shared_ptr<Shader>& p_shader, bool p_useCache )
    {
        LoadResult result;

        Assimp::Importer importer;

        unsigned int flags = aiProcess_CalcTangentSpace |
                             aiProcess_ImproveCacheLocality;

        const aiScene* scene = importer.ReadFile( p_filepath, flags );

        if ( !scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode )
        {
            LOG_ERROR() << "Assimp error: " << importer.GetErrorString();
            return result;
        }

        if ( scene->mNumMeshes == 0 )
        {
            LOG_ERROR() << "No meshes found in GLB file: " << p_filepath;
            return result;
        }

        // Create root node
        auto rootNode     = std::make_shared<Node>( "GLB_Root_" + p_filepath.substr( p_filepath.find_last_of( "/\\" ) + 1 ) );
        result.m_rootNode = rootNode;

        // Process all nodes recursively
        processNode( scene->mRootNode, scene, rootNode, p_shader, result.m_materials, p_useCache );

        return result;
    }

    std::shared_ptr<Node> GLBLoader::createInstance( const std::shared_ptr<Node>& p_original )
    {
        if ( !p_original ) return nullptr;

        // Create a deep copy of the node hierarchy
        return deepCopyNode( p_original );
    }

    std::shared_ptr<Node> GLBLoader::deepCopyNode( const std::shared_ptr<Node>& p_original )
    {
        auto copy = std::make_shared<Node>( p_original->m_name + "_Instance" );

        // Copy transform
        copy->m_transform = p_original->m_transform;

        for ( const auto& component : p_original->getComponents() )
        {
            if ( auto meshComp = std::dynamic_pointer_cast<MeshComponent>( component ) )
            {
                auto newMeshComp = std::make_shared<MeshComponent>( meshComp->getMesh(), meshComp->getMaterial() );
                copy->addComponent( newMeshComp );
            }
        }

        // Recursively copy children
        for ( const auto& child : p_original->getChildren() )
        {
            copy->addChild( deepCopyNode( child ) );
        }

        return copy;
    }

    void GLBLoader::processNode( const aiNode* p_node, const aiScene* p_scene, const std::shared_ptr<Node>& p_parentNode,
                                 const std::shared_ptr<Shader>& p_shader, std::vector<std::shared_ptr<Material>>& p_materials, bool p_useCache )
    {
        // Create a new node
        auto newNode = std::make_shared<Node>( p_node->mName.C_Str() );

        // Set transform
        aiMatrix4x4 transform = p_node->mTransformation;
        glm::mat4   matrix    = glm::mat4( transform.a1, transform.b1, transform.c1, transform.d1, transform.a2, transform.b2, transform.c2, transform.d2,
                                           transform.a3, transform.b3, transform.c3, transform.d3, transform.a4, transform.b4, transform.c4, transform.d4 );

        // Decompose matrix into translation, rotation, scale
        glm::vec3 scale;
        glm::quat rotation;
        glm::vec3 translation;
        glm::vec3 skew;
        glm::vec4 perspective;
        glm::decompose( matrix, scale, rotation, translation, skew, perspective );

        newNode->m_transform.m_translation = translation;
        newNode->m_transform.m_rotation    = rotation;
        newNode->m_transform.m_scale       = scale;
        newNode->m_transform.update();

        // Process meshes if this node has any
        for ( unsigned int i = 0; i < p_node->mNumMeshes; i++ )
        {
            aiMesh* mesh = p_scene->mMeshes[p_node->mMeshes[i]];
            processMesh( mesh, p_scene, newNode, p_shader, p_materials, p_useCache );
        }

        // Process children recursively
        for ( unsigned int i = 0; i < p_node->mNumChildren; i++ )
        {
            processNode( p_node->mChildren[i], p_scene, newNode, p_shader, p_materials, p_useCache );
        }

        p_parentNode->addChild( newNode );
    }

    void GLBLoader::processMesh( const aiMesh* p_mesh, const aiScene* p_scene, const std::shared_ptr<Node>& p_node, const std::shared_ptr<Shader>& p_shader,
                                 std::vector<std::shared_ptr<Material>>& p_materials, bool p_useCache )
    {
        // Generate a unique key for this mesh
        std::string meshKey = std::string( p_mesh->mName.C_Str() ) + "_mesh";

        std::shared_ptr<Mesh> meshComponent;

        // Check cache if enabled
        if ( p_useCache )
        {
            meshComponent = ResourceManager::getMesh( meshKey );
        }

        // If not in cache, create new mesh
        if ( !meshComponent )
        {
            // Extract vertex data
            std::vector<float>    positions;
            std::vector<float>    normals;
            std::vector<float>    texCoords;
            std::vector<float>    tangents;
            std::vector<uint32_t> indices;

            // Positions
            positions.reserve( p_mesh->mNumVertices * 3 );
            for ( unsigned int i = 0; i < p_mesh->mNumVertices; i++ )
            {
                positions.push_back( p_mesh->mVertices[i].x );
                positions.push_back( p_mesh->mVertices[i].y );
                positions.push_back( p_mesh->mVertices[i].z );
            }

            // Normals
            normals.reserve( p_mesh->mNumVertices * 3 );
            if ( p_mesh->HasNormals() )
            {
                for ( unsigned int i = 0; i < p_mesh->mNumVertices; i++ )
                {
                    normals.push_back( p_mesh->mNormals[i].x );
                    normals.push_back( p_mesh->mNormals[i].y );
                    normals.push_back( p_mesh->mNormals[i].z );
                }
            }

            // Texture Coordinates
            texCoords.reserve( p_mesh->mNumVertices * 2 );
            if ( p_mesh->HasTextureCoords( 0 ) )
            {
                for ( unsigned int i = 0; i < p_mesh->mNumVertices; i++ )
                {
                    texCoords.push_back( p_mesh->mTextureCoords[0][i].x );
                    texCoords.push_back( p_mesh->mTextureCoords[0][i].y );
                }
            }

            // Tangents
            tangents.reserve( p_mesh->mNumVertices * 4 );
            if ( p_mesh->HasTangentsAndBitangents() && p_mesh->HasNormals() )  // Ensure normals are also present for cross product
            {
                for ( unsigned int i = 0; i < p_mesh->mNumVertices; i++ )
                {
                    glm::vec3 assimpTangent   = glm::vec3( p_mesh->mTangents[i].x, p_mesh->mTangents[i].y, p_mesh->mTangents[i].z );
                    glm::vec3 assimpBitangent = glm::vec3( p_mesh->mBitangents[i].x, p_mesh->mBitangents[i].y, p_mesh->mBitangents[i].z );
                    glm::vec3 assimpNormal    = glm::vec3( p_mesh->mNormals[i].x, p_mesh->mNormals[i].y, p_mesh->mNormals[i].z );

                    glm::vec3 calculatedBitangent = glm::cross( assimpNormal, assimpTangent );
                    float     bitangentSign       = glm::dot( calculatedBitangent, assimpBitangent ) > 0.0f ? 1.0f : -1.0f;

                    tangents.push_back( assimpTangent.x );
                    tangents.push_back( assimpTangent.y );
                    tangents.push_back( assimpTangent.z );
                    tangents.push_back( bitangentSign );
                }
            }
            // If the mesh doesn't have tangents/bitangents, you might want to push a default vec4(0,0,0,1)
            // or ensure your shader handles this case by not applying normal mapping.
            else if ( !p_mesh->HasTangentsAndBitangents() && p_mesh->HasNormals() )
            {
                // If no tangents, but normals are present, push a default.
                // The shader should then ideally bypass normal map calculations.
                for ( unsigned int i = 0; i < p_mesh->mNumVertices; i++ )
                {
                    tangents.push_back( 0.0f );
                    tangents.push_back( 0.0f );
                    tangents.push_back( 0.0f );
                    tangents.push_back( 1.0f );  // Default sign to 1.0
                }
            }

            // Indices
            indices.reserve( p_mesh->mNumFaces * 3 );
            for ( unsigned int i = 0; i < p_mesh->mNumFaces; i++ )
            {
                aiFace face = p_mesh->mFaces[i];
                for ( unsigned int j = 0; j < face.mNumIndices; j++ )
                {
                    indices.push_back( face.mIndices[j] );
                }
            }

            // Create buffers
            auto positionBuffer = std::make_shared<VertexBuffer>( positions.data(), positions.size() * sizeof( float ) );

            auto normalBuffer = normals.empty() ? nullptr : std::make_shared<VertexBuffer>( normals.data(), normals.size() * sizeof( float ) );

            auto texCoordBuffer = texCoords.empty() ? nullptr : std::make_shared<VertexBuffer>( texCoords.data(), texCoords.size() * sizeof( float ) );

            auto tangentBuffer = tangents.empty() ? nullptr : std::make_shared<VertexBuffer>( tangents.data(), tangents.size() * sizeof( float ) );

            auto indexBuffer = std::make_shared<IndexBuffer>( indices.data(), indices.size() );

            // Create mesh
            meshComponent = std::make_shared<Mesh>( positionBuffer, normalBuffer, texCoordBuffer, tangentBuffer, indexBuffer, p_shader );

            // Add to cache if enabled
            if ( p_useCache )
            {
                ResourceManager::cacheMesh( meshKey, meshComponent );
            }
        }

        // Load material
        std::shared_ptr<Material> material;
        if ( p_mesh->mMaterialIndex >= 0 )
        {
            aiMaterial* aiMat = p_scene->mMaterials[p_mesh->mMaterialIndex];
            material          = loadMaterial( aiMat, p_shader, p_scene, p_useCache );
        }
        else
        {
            material = createDefaultMaterial( p_shader );
        }

        p_materials.push_back( material );

        // Create a MeshComponent and add it to the node
        auto meshComp = std::make_shared<MeshComponent>( meshComponent, material );
        p_node->addComponent( meshComp );
    }

    std::shared_ptr<Material> GLBLoader::loadMaterial( aiMaterial* p_aiMat, const std::shared_ptr<Shader>& p_shader, const aiScene* p_scene, bool p_useCache )
    {
        // Generate material key
        aiString matName;
        p_aiMat->Get( AI_MATKEY_NAME, matName );
        std::string materialKey =
                std::string( "GLB_Material_" ) + ( matName.length > 0 ? matName.C_Str() : std::to_string( ResourceManager::getNewResourceId() ) );

        // Check cache if enabled
        if ( p_useCache )
        {
            auto m = ResourceManager::getMaterial( materialKey );
            if ( m )
            {
                return m;
            }
        }

        // Base color texture
        std::shared_ptr<Texture> baseTexture = nullptr;
        aiString                 texPath;
        if ( p_aiMat->GetTexture( aiTextureType_DIFFUSE, 0, &texPath ) == AI_SUCCESS )
        {
            // Check for embedded texture
            if ( texPath.data[0] == '*' )
            {
                int texIndex = std::atoi( texPath.C_Str() + 1 );
                if ( texIndex >= 0 && texIndex < ( int ) p_scene->mNumTextures )
                {
                    baseTexture = loadTexture( p_scene->mTextures[texIndex], p_scene, p_useCache );
                }
            }
            else
            {
                // External texture (shouldn't happen with GLB)
                std::string fullPath = ResourceManager::getTexturePath( texPath.C_Str() );
                baseTexture          = Texture::createFromFile( fullPath, TextureFormat::RGBA16F );
            }
        }

        // Normal map
        std::shared_ptr<Texture> normalTexture = nullptr;
        if ( p_aiMat->GetTexture( aiTextureType_NORMALS, 0, &texPath ) == AI_SUCCESS || p_aiMat->GetTexture( aiTextureType_HEIGHT, 0, &texPath ) == AI_SUCCESS )
        {
            if ( texPath.data[0] == '*' )
            {
                int texIndex = std::atoi( texPath.C_Str() + 1 );
                if ( texIndex >= 0 && texIndex < ( int ) p_scene->mNumTextures )
                {
                    normalTexture = loadTexture( p_scene->mTextures[texIndex], p_scene, p_useCache );
                }
            }
        }

        // Metallic/roughness
        std::shared_ptr<Texture> metallicRoughnessTexture = nullptr;
        if ( p_aiMat->GetTexture( aiTextureType_UNKNOWN, 0, &texPath ) == AI_SUCCESS )
        {
            if ( texPath.data[0] == '*' )
            {
                int texIndex = std::atoi( texPath.C_Str() + 1 );
                if ( texIndex >= 0 && texIndex < ( int ) p_scene->mNumTextures )
                {
                    metallicRoughnessTexture = loadTexture( p_scene->mTextures[texIndex], p_scene, p_useCache );
                }
            }
        }

        // Emission texture
        std::shared_ptr<Texture> emissionTexture = nullptr;
        if ( p_aiMat->GetTexture( aiTextureType_EMISSIVE, 0, &texPath ) == AI_SUCCESS )
        {
            if ( texPath.data[0] == '*' )
            {
                int texIndex = std::atoi( texPath.C_Str() + 1 );
                if ( texIndex >= 0 && texIndex < ( int ) p_scene->mNumTextures )
                {
                    emissionTexture = loadTexture( p_scene->mTextures[texIndex], p_scene, p_useCache );
                }
            }
        }

        // Material properties
        aiColor3D baseColor( 1.0f, 1.0f, 1.0f );
        p_aiMat->Get( AI_MATKEY_COLOR_DIFFUSE, baseColor );

        float metallic = 0.0f;
        // Note: Assimp doesn't have direct metallic factor support, using default
        // p_aiMat->Get( AI_MATKEY_METALLIC_FACTOR, metallic );

        float roughness = 1.0f;
        // Note: Assimp doesn't have direct roughness factor support, using default
        // p_aiMat->Get( AI_MATKEY_ROUGHNESS_FACTOR, roughness );

        aiColor3D emission( 0.0f, 0.0f, 0.0f );
        p_aiMat->Get( AI_MATKEY_COLOR_EMISSIVE, emission );

        // Alpha mode - using standard Assimp properties
        AlphaMode alphaMode    = AlphaMode::OPAQUE;
        int       alphaModeInt = 0;
        // Note: Assimp doesn't have direct GLTF alpha mode support, using default
        // if ( p_aiMat->Get( AI_MATKEY_GLTF_ALPHAMODE, alphaModeInt ) == AI_SUCCESS )

        float alphaCutoff = 0.5f;
        // Note: Assimp doesn't have direct GLTF alpha cutoff support, using default
        // p_aiMat->Get( AI_MATKEY_GLTF_ALPHACUTOFF, alphaCutoff );

        auto material = std::make_shared<Material>( p_shader, glm::vec4( baseColor.r, baseColor.g, baseColor.b, 1.0f ),
                                                    glm::vec3( emission.r, emission.g, emission.b ), metallic, roughness,
                                                    1.0f,  // normal scale
                                                    alphaCutoff, alphaMode, baseTexture ? baseTexture : Material::getDefaultWhite(), metallicRoughnessTexture,
                                                    emissionTexture, normalTexture, Sampler::createLinearRepeat(), materialKey );

        // Add to cache if enabled
        if ( p_useCache )
        {
            ResourceManager::cacheMaterial( materialKey, material );
        }

        return material;
    }

    std::shared_ptr<Texture> GLBLoader::loadTexture( aiTexture* p_texture, const aiScene* p_scene, bool p_useCache )
    {
        if ( !p_texture )
        {
            return nullptr;
        }

        // Generate a unique key for this texture
        std::string textureKey = "embedded_texture_" + std::to_string( ( size_t ) p_texture );

        // Check cache if enabled
        if ( p_useCache )
        {
            auto cachedTexture = ResourceManager::getTexture( textureKey );
            if ( cachedTexture )
            {
                return cachedTexture;
            }
        }

        std::shared_ptr<Texture> textureObj = nullptr;

        // Handle compressed embedded textures (most common in GLB)
        if ( p_texture->mHeight == 0 )
        {
            // Texture is compressed (e.g., PNG/JPG data)
            textureObj = Texture::createFromMemory( reinterpret_cast<const unsigned char*>( p_texture->pcData ),
                                                    p_texture->mWidth,  // This contains the size for compressed textures
                                                    TextureFormat::SRGBA8 );
        }
        else
        {
            // Uncompressed texture (rare in GLB)
            // Note: Assimp stores uncompressed textures as RGBA8
            textureObj = Texture::createFromPixels( reinterpret_cast<const unsigned char*>( p_texture->pcData ), p_texture->mWidth, p_texture->mHeight,
                                                    TextureFormat::RGBA8 );
        }

        // Add to cache if enabled and loaded successfully
        if ( p_useCache && textureObj )
        {
            ResourceManager::cacheTexture( textureKey, textureObj );
        }

        return textureObj;
    }

    std::shared_ptr<Material> GLBLoader::createDefaultMaterial( const std::shared_ptr<Shader>& p_shader )
    {
        return std::make_shared<Material>( p_shader,
                                           glm::vec4( 1.0f ),  // white
                                           glm::vec3( 0.0f ),  // no emission
                                           0.0f,  // non-metallic
                                           1.0f,  // fully rough
                                           1.0f,  // normal scale
                                           0.5f,  // alpha cutoff
                                           AlphaMode::OPAQUE, Material::getDefaultWhite(), Material::getDefaultWhite(), nullptr, Material::getDefaultNormal(),
                                           Sampler::createLinearRepeat(), "DefaultMaterial" );
    }
}  // namespace Tomos
