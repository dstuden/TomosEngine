#pragma once

#include <functional>
#include <memory>
#include <nlohmann/json.hpp>
#include <string>
#include <unordered_map>
#include <vector>

#include "Tomos/core/scene/TSceneNode.hh"
#include "Tomos/core/scene/TSceneResourceBag.hh"
#include "Tomos/systems/TComponent.hh"
#include "Tomos/systems/asset/TAssetSystem.hh"

namespace Tomos
{
    class TVkGpu;
    class TSkinnedMeshComponent;

    struct TPendingSkinnedJoints
    {
        TSkinnedMeshComponent* m_component = nullptr;
        nlohmann::json         m_joints;
    };

    struct TComponentResolveCtx
    {
        TAssetSystem&      m_assets;
        TSceneResourceBag& m_bag;
        TVkGpu*            m_gpu = nullptr;

        std::unordered_map<uint64_t, std::shared_ptr<TSceneNode>>* m_nodesById = nullptr;

        std::vector<TPendingSkinnedJoints>* m_pendingSkinned = nullptr;
    };

    struct TComponentTypeInfo
    {
        std::string                                                                                m_type;
        std::string                                                                                m_label;
        std::function<std::shared_ptr<TComponent>()>                                               m_create;
        std::function<bool( const TComponent& )>                                                   m_matches;
        std::function<nlohmann::json( const TComponent&, const TComponentResolveCtx& )>            m_save;
        std::function<std::shared_ptr<TComponent>( const nlohmann::json&, TComponentResolveCtx& )> m_load;
    };

    class TComponentRegistry
    {
    public:
        static TComponentRegistry& get();

        void ensureDefaults();

        void registerType( TComponentTypeInfo p_info );

        [[nodiscard]] const std::vector<TComponentTypeInfo>& types() const { return m_types; }
        [[nodiscard]] const TComponentTypeInfo*              find( const std::string& p_type ) const;

        [[nodiscard]] std::string typeOf( const TComponent& p_component ) const;

        [[nodiscard]] nlohmann::json              saveComponent( const TComponent& p_component, const TComponentResolveCtx& p_ctx ) const;
        [[nodiscard]] std::shared_ptr<TComponent> loadComponent( const nlohmann::json& p_json, TComponentResolveCtx& p_ctx ) const;

        [[nodiscard]] std::shared_ptr<TComponent> createDefault( const std::string& p_type ) const;

        static void wireSkinnedJoints( std::vector<TPendingSkinnedJoints>& p_pending, const std::unordered_map<uint64_t, std::shared_ptr<TSceneNode>>& p_byId );

    private:
        TComponentRegistry() = default;
        std::vector<TComponentTypeInfo>         m_types;
        std::unordered_map<std::string, size_t> m_byType;
        bool                                    m_defaultsRegistered = false;
    };
}  // namespace Tomos
