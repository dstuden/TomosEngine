//
// Created by dstuden on 5/31/25.
//

#include "TResourceManager.hh"

#include "Tomos/util/logger/TLogger.hh"

namespace Tomos
{
    TResourceManager::ResourceCache TResourceManager::g_resourceCache = {};

    std::shared_ptr<TMesh> TResourceManager::getMesh( const std::string& p_meshName )
    {
        auto it = g_resourceCache.m_meshCache.find( p_meshName );
        if ( it != g_resourceCache.m_meshCache.end() )
        {
            return it->second;
        }

        return nullptr;
    }

    std::shared_ptr<TMaterial> TResourceManager::getMaterial( const std::string& p_materialName )
    {
        auto it = g_resourceCache.m_materialCache.find( p_materialName );
        if ( it != g_resourceCache.m_materialCache.end() )
        {
            return it->second;
        }

        return nullptr;
    }

    std::shared_ptr<TTexture> TResourceManager::getTexture( const std::string& p_textureName )
    {
        auto it = g_resourceCache.m_textureCache.find( p_textureName );
        if ( it != g_resourceCache.m_textureCache.end() )
        {
            return it->second;
        }

        return nullptr;
    }

    std::shared_ptr<TShader> TResourceManager::getShader( const std::string& p_shaderName )
    {
        auto it = g_resourceCache.m_shaderCache.find( p_shaderName );
        if ( it != g_resourceCache.m_shaderCache.end() )
        {
            return it->second;
        }

        return nullptr;
    }

    void TResourceManager::cacheMesh( const std::string& p_meshName, const std::shared_ptr<TMesh>& p_mesh )
    {
        if ( !p_mesh ) return;

        g_resourceCache.m_meshCache[p_meshName] = p_mesh;
        TLOG_DEBUG() << "Mesh added to cache: " << p_meshName;
    }

    void TResourceManager::cacheMaterial( const std::string& p_materialName, const std::shared_ptr<TMaterial>& p_material )
    {
        if ( !p_material ) return;

        g_resourceCache.m_materialCache[p_materialName] = p_material;
        TLOG_DEBUG() << "Material added to cache: " << p_materialName;
    }

    void TResourceManager::cacheShader( const std::string& p_shaderName, const std::shared_ptr<TShader>& p_shader )
    {
        if ( !p_shader ) return;

        g_resourceCache.m_shaderCache[p_shaderName] = p_shader;
        TLOG_DEBUG() << "Shader added to cache: " << p_shaderName;
    }

    void TResourceManager::cacheTexture( const std::string& p_textureName, const std::shared_ptr<TTexture>& p_texture )
    {
        if ( !p_texture ) return;

        g_resourceCache.m_textureCache[p_textureName] = p_texture;
        TLOG_DEBUG() << "Texture added to cache: " << p_textureName;
    }
}  // namespace Tomos
