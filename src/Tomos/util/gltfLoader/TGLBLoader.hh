#pragma once

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <memory>
#include <vector>

#include "Tomos/core/TNode.hh"
#include "Tomos/systems/mesh/TMesh.hh"
#include "Tomos/util/renderer/TMaterial.hh"
#include "Tomos/util/renderer/TTexture.hh"
#include "Tomos/util/resourceManager/TResourceManager.hh"

namespace Tomos
{
    class TGLBLoader
    {
    public:
        struct LoadResult
        {
            std::shared_ptr<TNode>                  m_rootNode;
            std::vector<std::shared_ptr<TMaterial>> m_materials;
        };

        static LoadResult loadGLB( const std::string& p_filepath, const std::shared_ptr<TShader>& p_shader, bool p_useCache = true );

        static std::shared_ptr<TNode> createInstance( const std::shared_ptr<TNode>& p_original );

    private:
        static void processNode( const aiNode* p_node, const aiScene* p_scene, const std::shared_ptr<TNode>& p_parentNode,
                                 const std::shared_ptr<TShader>& p_shader, std::vector<std::shared_ptr<TMaterial>>& p_materials, bool p_useCache );

        static void processMesh( const aiMesh* p_mesh, const aiScene* p_scene, const std::shared_ptr<TNode>& p_node, const std::shared_ptr<TShader>& p_shader,
                                 std::vector<std::shared_ptr<TMaterial>>& p_materials, bool p_useCache );

        static std::shared_ptr<TMaterial> loadMaterial( aiMaterial* p_aiMat, const std::shared_ptr<TShader>& p_shader, const aiScene* p_scene,
                                                        bool p_useCache );

        static std::shared_ptr<TTexture> loadTexture( aiTexture* p_texture, const aiScene* p_scene, bool p_useCache, TextureFormat p_format );

        static std::shared_ptr<TMaterial> createDefaultMaterial( const std::shared_ptr<TShader>& p_shader );

        static std::shared_ptr<TNode> deepCopyNode( const std::shared_ptr<TNode>& p_original );
    };
}  // namespace Tomos
