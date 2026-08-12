#pragma once

#include <glm/glm.hpp>
#include <memory>
#include <unordered_map>

#include "Tomos/systems/TSystem.hh"
#include "Tomos/systems/audio/TAudioComponent.hh"

namespace Tomos
{
    class TSceneNode;

    // Call updateListener() after lateUpdate (active camera).
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

        // Forward = view direction (local −Z).
        void updateListener( const glm::vec3& p_position, const glm::vec3& p_forward, const glm::vec3& p_up );

        void                setMasterVolume( float p_volume );
        [[nodiscard]] float masterVolume() const { return m_masterVolume; }

        [[nodiscard]] bool isReady() const { return m_ready; }

    private:
        struct TVoice;

        void destroyVoice( TAudioComponent* p_comp );
        bool ensureVoice( TAudioComponent* p_comp );
        void syncVoiceParams( TAudioComponent* p_comp, TSceneNode* p_node );

        std::unordered_map<TAudioComponent*, TSceneNode*>             m_emitters;
        std::unordered_map<TAudioComponent*, std::unique_ptr<TVoice>> m_voices;

        void* m_engine       = nullptr;
        bool  m_ready        = false;
        float m_masterVolume = 1.0f;
    };
}  // namespace Tomos
