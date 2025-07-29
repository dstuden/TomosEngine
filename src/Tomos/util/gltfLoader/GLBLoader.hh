#pragma once

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <memory>
#include <vector>

#include "Tomos/core/Node.hh"
#include "Tomos/systems/mesh/Mesh.hh"
#include "Tomos/util/renderer/Material.hh"
#include "Tomos/util/renderer/Texture.hh"
#include "Tomos/util/resourceManager/ResourceManager.hh"

namespace Tomos
{
    class GLBLoader
    {
    public:
        struct LoadResult
        {
            std::shared_ptr<Node>                  m_rootNode;
            std::vector<std::shared_ptr<Material>> m_materials;
        };

        static LoadResult loadGLB( const std::string& p_filepath, const std::shared_ptr<Shader>& p_shader, bool p_useCache = true );

        static std::shared_ptr<Node> createInstance( const std::shared_ptr<Node>& p_original );

    private:
        static void processNode( const aiNode* p_node, const aiScene* p_scene, const std::shared_ptr<Node>& p_parentNode,
                                 const std::shared_ptr<Shader>& p_shader, std::vector<std::shared_ptr<Material>>& p_materials, bool p_useCache );

        static void processMesh( const aiMesh* p_mesh, const aiScene* p_scene, const std::shared_ptr<Node>& p_node, const std::shared_ptr<Shader>& p_shader,
                                 std::vector<std::shared_ptr<Material>>& p_materials, bool p_useCache );

        static std::shared_ptr<Material> loadMaterial( aiMaterial* p_aiMat, const std::shared_ptr<Shader>& p_shader, const aiScene* p_scene, bool p_useCache );

        static std::shared_ptr<Texture>  loadTexture( aiTexture* p_texture, const aiScene* p_scene, bool p_useCache );
        static std::shared_ptr<Material> createDefaultMaterial( const std::shared_ptr<Shader>& p_shader );

        static std::shared_ptr<Node> deepCopyNode( const std::shared_ptr<Node>& p_original );
    };
}  // namespace Tomos
