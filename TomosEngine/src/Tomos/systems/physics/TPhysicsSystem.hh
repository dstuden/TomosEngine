#pragma once

#include <functional>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "Tomos/systems/TSystem.hh"
#include "Tomos/systems/physics/TColliderComponent.hh"
#include "Tomos/systems/physics/TRigidBodyComponent.hh"

namespace Tomos
{
    class TSceneNode;
    class TTime;

    // Gaffer fixed timestep + render interpolation. Register after TScriptSystem.
    class TPhysicsSystem : public TSystem
    {
    public:
        static constexpr float g_kMaxFrame = 0.25f;
        static constexpr int   g_kMaxSteps = 8;
        // Below this inbound |vn|, cancel normal instead of bouncing (rest jitter).
        static constexpr float g_kRestNormalSpeed = 0.5f;

        glm::vec3 m_gravity{ 0.0f, -9.81f, 0.0f };
        float     m_broadphaseCellSize = 2.0f;

        using TOverlapListener = std::function<void( TSceneNode& p_a, TSceneNode& p_b, TOverlapPhase p_phase )>;

        [[nodiscard]] bool matches( const TComponent& p_component ) const override;

        void componentCreated( TSceneNode& p_node, TComponent& p_component ) override;
        void componentDestroyed( TSceneNode& p_node, TComponent& p_component ) override;

        void update( float p_dt ) override;

        void setOverlapListener( TOverlapListener p_listener ) { m_overlapListener = std::move( p_listener ); }

        void setTime( const TTime* p_time ) { m_time = p_time; }

        [[nodiscard]] float fixedDt() const;
        [[nodiscard]] float accumulator() const { return m_accumulator; }
        [[nodiscard]] float alpha() const { return m_alpha; }
        [[nodiscard]] int   lastStepCount() const { return m_lastStepCount; }
        [[nodiscard]] int   lastContactCount() const { return m_lastContactCount; }
        [[nodiscard]] int   lastTriggerEventCount() const { return m_lastTriggerEventCount; }
        [[nodiscard]] int   activeOverlapCount() const { return static_cast<int>( m_activeOverlaps.size() ); }

        [[nodiscard]] size_t bodyCount() const { return m_bodies.size(); }
        [[nodiscard]] size_t colliderCount() const { return m_colliders.size(); }

        struct TBodyDebug
        {
            TSceneNode*                m_node = nullptr;
            const TRigidBodyComponent* m_body = nullptr;
            glm::vec3                  m_simPosition{ 0.0f };
        };

        struct TColliderDebug
        {
            TSceneNode*               m_node     = nullptr;
            const TColliderComponent* m_collider = nullptr;
        };

        void forEachBody( const std::function<void( const TBodyDebug& )>& p_fn ) const;
        void forEachCollider( const std::function<void( const TColliderDebug& )>& p_fn ) const;

        [[nodiscard]] static bool colliderWorldAabb( const TSceneNode& p_node, const TColliderComponent& p_col, glm::vec3& p_min, glm::vec3& p_max );

    private:
        struct TBodyEntry
        {
            TSceneNode*          m_node = nullptr;
            TRigidBodyComponent* m_body = nullptr;
            glm::vec3            m_position{ 0.0f };
            glm::vec3            m_previousPosition{ 0.0f };
            glm::vec3            m_lastWritten{ 0.0f };
        };

        struct TColliderEntry
        {
            TSceneNode*         m_node     = nullptr;
            TColliderComponent* m_collider = nullptr;
        };

        struct TWorldAABB
        {
            glm::vec3 m_min{ 0.0f };
            glm::vec3 m_max{ 0.0f };
        };

        struct TWorldPose
        {
            glm::vec3 m_center{ 0.0f };
            glm::quat m_rotation{ 1.0f, 0.0f, 0.0f, 0.0f };
            glm::vec3 m_scale{ 1.0f };
        };

        struct TContact
        {
            glm::vec3 m_normal{ 0.0f };  // from B toward A
            float     m_penetration = 0.0f;
        };

        struct TOverlapKey
        {
            TColliderComponent* m_a = nullptr;
            TColliderComponent* m_b = nullptr;

            bool operator==( const TOverlapKey& p_o ) const { return m_a == p_o.m_a && m_b == p_o.m_b; }
        };

        struct TOverlapKeyHash
        {
            size_t operator()( const TOverlapKey& p_k ) const noexcept
            {
                const auto ha = std::hash<const void*>{}( p_k.m_a );
                const auto hb = std::hash<const void*>{}( p_k.m_b );
                return ha ^ ( hb + 0x9e3779b9 + ( ha << 6 ) + ( ha >> 2 ) );
            }
        };

        void step( float p_dt );
        void integrateBodies( float p_dt );
        void collideAndResolve();
        void writeTransforms();
        void finishOverlapFrame( const std::unordered_set<TOverlapKey, TOverlapKeyHash>& p_current );
        void emitOverlap( TColliderComponent& p_a, TSceneNode& p_nodeA, TColliderComponent& p_b, TSceneNode& p_nodeB, TOverlapPhase p_phase );

        [[nodiscard]] static TColliderComponent*  colliderOn( TSceneNode* p_node );
        [[nodiscard]] static TRigidBodyComponent* bodyOn( TSceneNode* p_node );
        [[nodiscard]] bool                        isStaticCollider( const TColliderEntry& p_entry ) const;
        [[nodiscard]] static bool                 isDynamicBody( const TBodyEntry& p_entry );

        static TOverlapKey makeOverlapKey( TColliderComponent* p_a, TColliderComponent* p_b );
        static TWorldPose  worldPose( const TSceneNode& p_node );
        static void        setWorldTranslation( TSceneNode& p_node, const glm::vec3& p_worldPos );
        static TWorldAABB  worldAabb( const TWorldPose& p_pose, const TColliderComponent& p_col );
        static TWorldAABB  worldAabbSphere( const glm::vec3& p_center, float p_radius );
        static bool        computeContact( const glm::vec3& p_centerA, const glm::quat& p_rotA, const glm::vec3& p_scaleA, const TColliderComponent& p_colA,
                                           const glm::vec3& p_centerB, const glm::quat& p_rotB, const glm::vec3& p_scaleB, const TColliderComponent& p_colB,
                                           TContact& p_out );
        static void        applyContactImpulse( TRigidBodyComponent& p_body, const glm::vec3& p_normal, float p_restitution );
        static void resolveDynamicVsStatic( TBodyEntry& p_dyn, const TColliderComponent& p_dynCol, const glm::vec3& p_dynScale, const glm::quat& p_dynRot,
                                            const TWorldPose& p_staticPose, const TColliderComponent& p_staticCol );
        static void resolveDynamicVsDynamic( TBodyEntry& p_a, const TColliderComponent& p_colA, const glm::vec3& p_scaleA, const glm::quat& p_rotA,
                                             TBodyEntry& p_b, const TColliderComponent& p_colB, const glm::vec3& p_scaleB, const glm::quat& p_rotB );

        std::unordered_map<TRigidBodyComponent*, TBodyEntry>    m_bodies;
        std::unordered_map<TColliderComponent*, TColliderEntry> m_colliders;
        std::unordered_set<TOverlapKey, TOverlapKeyHash>        m_activeOverlaps;
        // Cleared each broadphase; capacity retained.
        std::unordered_map<uint64_t, std::vector<int>>   m_scratchCells;
        std::unordered_set<uint64_t>                     m_scratchPairKeys;
        std::unordered_set<TOverlapKey, TOverlapKeyHash> m_scratchOverlaps;
        TOverlapListener                                 m_overlapListener;
        const TTime*                                     m_time = nullptr;

        float m_accumulator           = 0.0f;
        float m_alpha                 = 0.0f;
        int   m_lastStepCount         = 0;
        int   m_lastContactCount      = 0;
        int   m_lastTriggerEventCount = 0;
    };
}  // namespace Tomos
