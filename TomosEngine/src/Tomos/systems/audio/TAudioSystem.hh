#pragma once

#include <glm/glm.hpp>
#include <memory>
#include <string>
#include <unordered_map>

#include "Tomos/systems/TSystem.hh"
#include "Tomos/systems/audio/TAudioComponent.hh"

namespace Tomos
{
    class TSceneNode;
    class TAudioClip;

    // Call updateListener() before lateUpdate (active camera).
    class TAudioSystem : public TTypedSystem<TAudioComponent>
    {
    public:
        TAudioSystem();
        ~TAudioSystem() override;

        TAudioSystem( const TAudioSystem& )            = delete;
        TAudioSystem& operator=( const TAudioSystem& ) = delete;

        void componentCreated( TSceneNode& p_node, TComponent& p_component ) override;
        void componentDestroyed( TSceneNode& p_node, TComponent& p_component ) override;

        void lateUpdate( float p_dt ) override;

        // Decode once into a shared cache (path-keyed). Safe to call repeatedly.
        bool preload( const TAudioClip* p_clip );
        bool preloadPath( const std::string& p_path );

        // Forward = view direction (local −Z).
        void updateListener( const glm::vec3& p_position, const glm::vec3& p_forward, const glm::vec3& p_up );

        void                setMasterVolume( float p_volume );
        [[nodiscard]] float masterVolume() const { return m_masterVolume; }

        [[nodiscard]] bool isReady() const { return m_ready; }

    private:
        struct TVoice;
        struct TCachedClip;

        void destroyVoice( TAudioComponent* p_comp );
        bool ensureVoice( TAudioComponent* p_comp );
        void syncVoiceParams( TAudioComponent* p_comp, TSceneNode* p_node );
        void clearClipCache();

        std::unordered_map<TAudioComponent*, TSceneNode*>             m_emitters;
        std::unordered_map<TAudioComponent*, std::unique_ptr<TVoice>> m_voices;
        std::unordered_map<std::string, std::unique_ptr<TCachedClip>> m_clipCache;

        void*     m_engine       = nullptr;
        bool      m_ready        = false;
        float     m_masterVolume = 1.0f;
        glm::vec3 m_listenerPos{};
        bool      m_hasListener = false;
    };
}  // namespace Tomos
