#pragma once

#include <filesystem>
#include <string>
#include <unordered_map>

namespace fs = std::filesystem;

namespace Tomos
{
    class TMesh;
    class TMaterial;
    class TTexture;
    class TShader;

    // NOLINTBEGIN
    /**
     * When adding new shader programs, update the LAST_VALUE to the highest value.
     * When extending this "enum" in your own project, ensure that the values are LAST_VALUE + 1, LAST_VALUE + 2, etc.
     */
    namespace TShaderPrograms
    {
        typedef int TSHaderProgram;

        inline const TSHaderProgram GBuff      = 0;
        inline const TSHaderProgram Light      = 1;
        inline const TSHaderProgram Screen     = 2;
        inline const TSHaderProgram LAST_VALUE = 2;  // Update this when adding new shader programs
    }  // namespace TShaderPrograms
    // NOLINTEND

    /**
     * In your just append new shader paths to this map.
     */
    namespace TShaderPrograms
    {
        inline std::unordered_map<TSHaderProgram, std::pair<std::string, std::string>> shaderPaths = {
                { GBuff, { "gbuff_vertex.glsl", "gbuff_fragment.glsl" } },
                { Light, { "screen_quad_vertex.glsl", "light_fragment.glsl" } },
                { Screen, { "screen_quad_vertex.glsl", "screen_quad_fragment.glsl" } } };
    }

    class TResourceManager
    {
    public:
        struct ResourceCache
        {
            std::unordered_map<std::string, std::shared_ptr<TMesh>>                       m_meshCache;
            std::unordered_map<std::string, std::shared_ptr<TMaterial>>                   m_materialCache;
            std::unordered_map<std::string, std::shared_ptr<TTexture>>                    m_textureCache;
            std::unordered_map<TShaderPrograms::TSHaderProgram, std::shared_ptr<TShader>> m_shaderCache;
        };

        static fs::path getShaderPath( const std::string& p_shaderName ) { return getBasePath() / "shaders" / ( p_shaderName ); }

        static fs::path getTexturePath( const std::string& p_textureName ) { return getBasePath() / "textures" / ( p_textureName ); }

        static fs::path GetModelPath( const std::string& p_modelName ) { return getBasePath() / "models" / ( p_modelName ); }

        static fs::path getBasePath() { return "resources"; }

        static unsigned int getNewResourceId()
        {
            static unsigned int id = 0;
            return id++;
        }

        static void clearCache()
        {
            g_resourceCache.m_meshCache.clear();
            g_resourceCache.m_materialCache.clear();
            g_resourceCache.m_textureCache.clear();
            g_resourceCache.m_shaderCache.clear();
        }

        static std::shared_ptr<TMesh>     getMesh( const std::string& p_meshName );
        static std::shared_ptr<TMaterial> getMaterial( const std::string& p_materialName );
        static std::shared_ptr<TTexture>  getTexture( const std::string& p_textureName );
        static std::shared_ptr<TShader>   getShader( TShaderPrograms::TSHaderProgram p_shaderType );

        static void cacheMesh( const std::string& p_meshName, const std::shared_ptr<TMesh>& p_mesh );
        static void cacheMaterial( const std::string& p_materialName, const std::shared_ptr<TMaterial>& p_material );
        static void cacheTexture( const std::string& p_textureName, const std::shared_ptr<TTexture>& p_texture );

        static void loadShader( TShaderPrograms::TSHaderProgram p_shaderType );

    protected:
        static ResourceCache g_resourceCache;
    };
}  // namespace Tomos
