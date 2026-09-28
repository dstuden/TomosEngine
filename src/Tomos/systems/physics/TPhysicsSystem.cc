#include "Tomos/systems/physics/TPhysicsSystem.hh"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <limits>
#include <unordered_map>
#include <utility>
#include <vector>

#include "Tomos/core/scene/TSceneNode.hh"
#include "Tomos/util/memory/TArenaAllocator.hh"
#include "Tomos/util/memory/TFrameAllocator.hh"
#include "Tomos/util/time/TTime.hh"

namespace Tomos
{
    namespace
    {
        constexpr float g_kEpsilon = 1e-6f;

        glm::vec3 obbWorldHalfExtents( const glm::quat& p_rot, const glm::vec3& p_localHalf )
        {
            const glm::mat3 r = glm::mat3_cast( p_rot );
            return glm::abs( r[ 0 ] ) * p_localHalf.x + glm::abs( r[ 1 ] ) * p_localHalf.y + glm::abs( r[ 2 ] ) * p_localHalf.z;
        }

        glm::vec3 scaledHalfExtents( const glm::vec3& p_localHalf, const glm::vec3& p_scale )
        {
            // Abs so mirrored scales still produce a positive volume.
            return glm::abs( p_localHalf * p_scale );
        }

        float scaledRadius( float p_localRadius, const glm::vec3& p_scale )
        {
            const glm::vec3 a = glm::abs( p_scale );
            return p_localRadius * std::max( { a.x, a.y, a.z } );
        }

        struct TBroadAabb
        {
            glm::vec3 m_min{ 0.0f };
            glm::vec3 m_max{ 0.0f };
        };

        bool aabbOverlap( const TBroadAabb& p_a, const TBroadAabb& p_b )
        {
            return !( p_a.m_max.x < p_b.m_min.x || p_a.m_min.x > p_b.m_max.x || p_a.m_max.y < p_b.m_min.y || p_a.m_min.y > p_b.m_max.y ||
                      p_a.m_max.z < p_b.m_min.z || p_a.m_min.z > p_b.m_max.z );
        }

        // Sphere vs oriented box.  Normal points from box toward sphere (outward).
        // p_half is already world-scaled local half-extents.
        bool sphereVsObb( const glm::vec3& p_sphere, float p_radius, const glm::vec3& p_boxCenter, const glm::quat& p_boxRot, const glm::vec3& p_half,
                          glm::vec3& p_normal, float& p_penetration )
        {
            const glm::quat invR    = glm::conjugate( glm::normalize( p_boxRot ) );
            const glm::vec3 local   = invR * ( p_sphere - p_boxCenter );
            const glm::vec3 closest = glm::clamp( local, -p_half, p_half );
            const glm::vec3 delta   = local - closest;
            const float     distSq  = glm::dot( delta, delta );

            glm::vec3 localNormal{ 0.0f };

            if ( distSq < g_kEpsilon )
            {
                const glm::vec3 toMin = local + p_half;
                const glm::vec3 toMax = p_half - local;
                const float     dx    = std::min( toMin.x, toMax.x );
                const float     dy    = std::min( toMin.y, toMax.y );
                const float     dz    = std::min( toMin.z, toMax.z );
                if ( dx <= dy && dx <= dz )
                {
                    localNormal   = ( toMin.x < toMax.x ) ? glm::vec3( -1, 0, 0 ) : glm::vec3( 1, 0, 0 );
                    p_penetration = p_radius + dx;
                }
                else if ( dy <= dz )
                {
                    localNormal   = ( toMin.y < toMax.y ) ? glm::vec3( 0, -1, 0 ) : glm::vec3( 0, 1, 0 );
                    p_penetration = p_radius + dy;
                }
                else
                {
                    localNormal   = ( toMin.z < toMax.z ) ? glm::vec3( 0, 0, -1 ) : glm::vec3( 0, 0, 1 );
                    p_penetration = p_radius + dz;
                }
            }
            else if ( distSq < p_radius * p_radius )
            {
                const float dist = std::sqrt( distSq );
                localNormal      = delta / dist;
                p_penetration    = p_radius - dist;
            }
            else
            {
                return false;
            }

            p_normal = glm::normalize( p_boxRot * localNormal );
            return p_penetration > 0.0f;
        }

        bool sphereVsSphere( const glm::vec3& p_a, float p_ra, const glm::vec3& p_b, float p_rb, glm::vec3& p_normal, float& p_penetration )
        {
            glm::vec3   delta  = p_a - p_b;
            float       distSq = glm::dot( delta, delta );
            const float rSum   = p_ra + p_rb;
            if ( distSq >= rSum * rSum ) return false;

            float dist = std::sqrt( std::max( distSq, g_kEpsilon ) );
            p_normal   = delta / dist;
            if ( distSq < g_kEpsilon ) p_normal = glm::vec3( 0, 1, 0 );
            p_penetration = rSum - dist;
            return p_penetration > 0.0f;
        }

        // OBB–OBB via SAT. Normal points from B toward A.
        bool obbVsObb( const glm::vec3& p_cA, const glm::quat& p_rotA, const glm::vec3& p_halfA, const glm::vec3& p_cB, const glm::quat& p_rotB,
                       const glm::vec3& p_halfB, glm::vec3& p_normal, float& p_penetration )
        {
            const glm::mat3 ra = glm::mat3_cast( glm::normalize( p_rotA ) );
            const glm::mat3 rb = glm::mat3_cast( glm::normalize( p_rotB ) );

            glm::vec3 axis[ 15 ];
            axis[ 0 ] = ra[ 0 ];
            axis[ 1 ] = ra[ 1 ];
            axis[ 2 ] = ra[ 2 ];
            axis[ 3 ] = rb[ 0 ];
            axis[ 4 ] = rb[ 1 ];
            axis[ 5 ] = rb[ 2 ];
            int nAxes = 6;
            for ( int i = 0; i < 3; ++i )
            {
                for ( int j = 0; j < 3; ++j )
                {
                    const glm::vec3 cross = glm::cross( ra[ i ], rb[ j ] );
                    const float     lenSq = glm::dot( cross, cross );
                    if ( lenSq > g_kEpsilon ) axis[ nAxes++ ] = cross / std::sqrt( lenSq );
                }
            }

            const glm::vec3 t = p_cA - p_cB;

            float     minPen = std::numeric_limits<float>::infinity();
            glm::vec3 bestAxis{ 0.0f, 1.0f, 0.0f };

            auto projectRadius = []( const glm::mat3& p_r, const glm::vec3& p_half, const glm::vec3& p_ax )
            {
                return p_half.x * std::abs( glm::dot( p_r[ 0 ], p_ax ) ) + p_half.y * std::abs( glm::dot( p_r[ 1 ], p_ax ) ) +
                       p_half.z * std::abs( glm::dot( p_r[ 2 ], p_ax ) );
            };

            for ( int i = 0; i < nAxes; ++i )
            {
                glm::vec3   a   = axis[ i ];
                const float len = glm::length( a );
                if ( len < g_kEpsilon ) continue;
                a /= len;

                const float dist = std::abs( glm::dot( t, a ) );
                const float raP  = projectRadius( ra, p_halfA, a );
                const float rbP  = projectRadius( rb, p_halfB, a );
                const float pen  = raP + rbP - dist;
                if ( pen <= 0.0f ) return false;
                if ( pen < minPen )
                {
                    minPen   = pen;
                    bestAxis = a;
                }
            }

            if ( glm::dot( t, bestAxis ) < 0.0f ) bestAxis = -bestAxis;
            p_normal      = bestAxis;
            p_penetration = minPen;
            return minPen > 0.0f && minPen < std::numeric_limits<float>::infinity();
        }
    }  // namespace

    bool TPhysicsSystem::matches( const TComponent& p_component ) const
    {
        return dynamic_cast<const TRigidBodyComponent*>( &p_component ) != nullptr || dynamic_cast<const TColliderComponent*>( &p_component ) != nullptr;
    }

    float TPhysicsSystem::fixedDt() const { return m_time != nullptr ? m_time->fixedDt() : TTime::g_kDefaultFixedDt; }

    void TPhysicsSystem::componentCreated( TSceneNode& p_node, TComponent& p_component )
    {
        if ( auto* body = dynamic_cast<TRigidBodyComponent*>( &p_component ) )
        {
            TBodyEntry entry{};
            entry.m_node             = &p_node;
            entry.m_body             = body;
            entry.m_position         = worldPose( p_node ).m_center;
            entry.m_previousPosition = entry.m_position;
            entry.m_lastWritten      = entry.m_position;
            m_bodies[ body ]         = entry;
            return;
        }

        if ( auto* col = dynamic_cast<TColliderComponent*>( &p_component ) )
        {
            m_colliders[ col ] = TColliderEntry{ &p_node, col };
        }
    }

    void TPhysicsSystem::componentDestroyed( TSceneNode& /*p_node*/, TComponent& p_component )
    {
        if ( auto* body = dynamic_cast<TRigidBodyComponent*>( &p_component ) )
        {
            m_bodies.erase( body );
            return;
        }
        if ( auto* col = dynamic_cast<TColliderComponent*>( &p_component ) )
        {
            for ( auto it = m_activeOverlaps.begin(); it != m_activeOverlaps.end(); )
            {
                if ( it->m_a == col || it->m_b == col )
                {
                    // Emit Exit while both collider entries are still registered.
                    auto aIt = m_colliders.find( it->m_a );
                    auto bIt = m_colliders.find( it->m_b );
                    if ( aIt != m_colliders.end() && bIt != m_colliders.end() )
                        emitOverlap( *aIt->second.m_collider, *aIt->second.m_node, *bIt->second.m_collider, *bIt->second.m_node,
                                     TOverlapPhase::Exit );
                    it = m_activeOverlaps.erase( it );
                }
                else
                    ++it;
            }
            m_colliders.erase( col );
        }
    }

    void TPhysicsSystem::update( float p_dt )
    {
        // Authoritative sim pose lives on the entry (world space). Dynamic bodies
        // normally ignore the node (it holds last frame's interpolated render pose),
        // but an external write (inspector / teleportTo / script) is detected by
        // comparing world translation against the last position we ourselves wrote.
        for ( auto& [ body, entry ] : m_bodies )
        {
            ( void ) body;
            if ( entry.m_body->hasPendingTeleport() )
            {
                const glm::vec3 pos      = entry.m_body->takePendingTeleport();
                entry.m_position         = pos;
                entry.m_previousPosition = pos;
                entry.m_lastWritten      = pos;
                setWorldTranslation( *entry.m_node, pos );
                continue;
            }

            const glm::vec3 nodeWorld = worldPose( *entry.m_node ).m_center;

            if ( !entry.m_body->isDynamic() )
            {
                entry.m_position         = nodeWorld;
                entry.m_previousPosition = entry.m_position;
                entry.m_lastWritten      = entry.m_position;
                continue;
            }

            // Parented dynamics can accumulate float error vs lastWritten — use epsilon.
            const glm::vec3 delta = nodeWorld - entry.m_lastWritten;
            if ( glm::dot( delta, delta ) > g_kEpsilon * g_kEpsilon )
            {
                entry.m_position         = nodeWorld;
                entry.m_previousPosition = nodeWorld;
                entry.m_lastWritten      = nodeWorld;
            }
        }

        const float frameTime = std::min( p_dt, g_kMaxFrame );
        m_accumulator += frameTime;

        const float stepDt      = fixedDt();
        m_lastStepCount         = 0;
        m_lastTriggerEventCount = 0;
        while ( m_accumulator >= stepDt && m_lastStepCount < g_kMaxSteps )
        {
            // Gaffer: previousState = currentState, then integrate current.
            for ( auto& [ body, entry ] : m_bodies )
            {
                ( void ) body;
                if ( entry.m_body->isDynamic() ) entry.m_previousPosition = entry.m_position;
            }

            step( stepDt );
            m_accumulator -= stepDt;
            ++m_lastStepCount;
        }

        m_alpha = m_accumulator / stepDt;
        writeTransforms();
    }

    void TPhysicsSystem::step( float p_dt )
    {
        integrateBodies( p_dt );
        collideAndResolve();
    }

    void TPhysicsSystem::integrateBodies( float p_dt )
    {
        for ( auto& [ bodyPtr, entry ] : m_bodies )
        {
            ( void ) bodyPtr;
            TRigidBodyComponent& body = *entry.m_body;
            if ( !body.isDynamic() )
            {
                body.clearForces();
                continue;
            }

            glm::vec3 force = body.m_force;
            if ( body.m_useGravity ) force += m_gravity * body.m_mass * body.m_gravityScale;

            body.m_linearVelocity += force * body.m_invMass * p_dt;

            const float damp = std::clamp( 1.0f - body.m_linearDamping, 0.0f, 1.0f );
            body.m_linearVelocity *= std::pow( damp, p_dt * 60.0f );

            entry.m_position += body.m_linearVelocity * p_dt;
            body.clearForces();
        }
    }

    TColliderComponent* TPhysicsSystem::colliderOn( TSceneNode* p_node )
    {
        if ( p_node == nullptr ) return nullptr;
        return p_node->findComponent<TColliderComponent>();
    }

    TRigidBodyComponent* TPhysicsSystem::bodyOn( TSceneNode* p_node )
    {
        if ( p_node == nullptr ) return nullptr;
        return p_node->findComponent<TRigidBodyComponent>();
    }

    bool TPhysicsSystem::isStaticCollider( const TColliderEntry& p_entry ) const
    {
        if ( p_entry.m_collider == nullptr || !p_entry.m_collider->m_enabled ) return false;
        const TRigidBodyComponent* body = bodyOn( p_entry.m_node );
        if ( body == nullptr ) return true;
        return !body->isDynamic();
    }

    bool TPhysicsSystem::isDynamicBody( const TBodyEntry& p_entry ) { return p_entry.m_body != nullptr && p_entry.m_body->isDynamic(); }

    TPhysicsSystem::TOverlapKey TPhysicsSystem::makeOverlapKey( TColliderComponent* p_a, TColliderComponent* p_b )
    {
        if ( p_a > p_b ) std::swap( p_a, p_b );
        return TOverlapKey{ p_a, p_b };
    }

    TPhysicsSystem::TWorldPose TPhysicsSystem::worldPose( const TSceneNode& p_node )
    {
        const glm::mat4 world = p_node.worldMatrix();
        TWorldPose      pose{};
        pose.m_center = glm::vec3( world[ 3 ] );

        auto        x  = glm::vec3( world[ 0 ] );
        auto        y  = glm::vec3( world[ 1 ] );
        auto        z  = glm::vec3( world[ 2 ] );
        const float lx = glm::length( x );
        const float ly = glm::length( y );
        const float lz = glm::length( z );
        pose.m_scale   = { lx, ly, lz };
        if ( lx > g_kEpsilon ) x /= lx;
        if ( ly > g_kEpsilon ) y /= ly;
        if ( lz > g_kEpsilon ) z /= lz;
        pose.m_rotation = glm::normalize( glm::quat_cast( glm::mat3( x, y, z ) ) );
        return pose;
    }

    void TPhysicsSystem::setWorldTranslation( TSceneNode& p_node, const glm::vec3& p_worldPos )
    {
        if ( auto parent = p_node.getParent() )
        {
            const glm::mat4 parentWorld = parent->worldMatrix();
            p_node.m_transform.setTranslation( glm::vec3( glm::inverse( parentWorld ) * glm::vec4( p_worldPos, 1.0f ) ) );
        }
        else
        {
            p_node.m_transform.setTranslation( p_worldPos );
        }
    }

    TPhysicsSystem::TWorldAABB TPhysicsSystem::worldAabb( const TWorldPose& p_pose, const TColliderComponent& p_col )
    {
        TWorldAABB box{};
        if ( p_col.m_shape == TColliderShape::Sphere )
        {
            const glm::vec3 r( scaledRadius( p_col.m_radius, p_pose.m_scale ) );
            box.m_min = p_pose.m_center - r;
            box.m_max = p_pose.m_center + r;
        }
        else
        {
            const glm::vec3 wh = obbWorldHalfExtents( p_pose.m_rotation, scaledHalfExtents( p_col.m_halfExtents, p_pose.m_scale ) );
            box.m_min          = p_pose.m_center - wh;
            box.m_max          = p_pose.m_center + wh;
        }
        return box;
    }

    TPhysicsSystem::TWorldAABB TPhysicsSystem::worldAabbSphere( const glm::vec3& p_center, float p_radius )
    {
        const glm::vec3 r( p_radius );
        return { p_center - r, p_center + r };
    }

    bool TPhysicsSystem::computeContact( const glm::vec3& p_centerA, const glm::quat& p_rotA, const glm::vec3& p_scaleA, const TColliderComponent& p_colA,
                                         const glm::vec3& p_centerB, const glm::quat& p_rotB, const glm::vec3& p_scaleB, const TColliderComponent& p_colB,
                                         TContact& p_out )
    {
        const float     ra    = scaledRadius( p_colA.m_radius, p_scaleA );
        const float     rb    = scaledRadius( p_colB.m_radius, p_scaleB );
        const glm::vec3 halfA = scaledHalfExtents( p_colA.m_halfExtents, p_scaleA );
        const glm::vec3 halfB = scaledHalfExtents( p_colB.m_halfExtents, p_scaleB );

        glm::vec3 normal{ 0.0f };
        float     penetration = 0.0f;
        bool      hit         = false;

        if ( p_colA.m_shape == TColliderShape::Sphere && p_colB.m_shape == TColliderShape::Sphere )
        {
            hit = sphereVsSphere( p_centerA, ra, p_centerB, rb, normal, penetration );
        }
        else if ( p_colA.m_shape == TColliderShape::Sphere && p_colB.m_shape == TColliderShape::Box )
        {
            hit = sphereVsObb( p_centerA, ra, p_centerB, p_rotB, halfB, normal, penetration );
        }
        else if ( p_colA.m_shape == TColliderShape::Box && p_colB.m_shape == TColliderShape::Sphere )
        {
            // Reuse sphere-vs-obb; flip normal so it still points B → A.
            hit = sphereVsObb( p_centerB, rb, p_centerA, p_rotA, halfA, normal, penetration );
            if ( hit ) normal = -normal;
        }
        else if ( p_colA.m_shape == TColliderShape::Box && p_colB.m_shape == TColliderShape::Box )
        {
            hit = obbVsObb( p_centerA, p_rotA, halfA, p_centerB, p_rotB, halfB, normal, penetration );
        }

        if ( !hit || penetration <= 0.0f ) return false;
        p_out.m_normal      = normal;
        p_out.m_penetration = penetration;
        return true;
    }

    void TPhysicsSystem::applyContactImpulse( TRigidBodyComponent& p_body, const glm::vec3& p_normal, float p_restitution )
    {
        const float vn = glm::dot( p_body.m_linearVelocity, p_normal );
        if ( vn >= 0.0f ) return;
        const float bounce = ( -vn >= g_kRestNormalSpeed ) ? p_restitution : 0.0f;
        p_body.m_linearVelocity -= ( 1.0f + bounce ) * vn * p_normal;
    }

    void TPhysicsSystem::resolveDynamicVsStatic( TBodyEntry& p_dyn, const TColliderComponent& p_dynCol, const glm::vec3& p_dynScale, const glm::quat& p_dynRot,
                                                 const TWorldPose& p_staticPose, const TColliderComponent& p_staticCol )
    {
        TContact contact{};
        if ( !computeContact( p_dyn.m_position, p_dynRot, p_dynScale, p_dynCol, p_staticPose.m_center, p_staticPose.m_rotation, p_staticPose.m_scale,
                              p_staticCol, contact ) )
            return;

        p_dyn.m_position += contact.m_normal * contact.m_penetration;
        applyContactImpulse( *p_dyn.m_body, contact.m_normal, p_dyn.m_body->m_restitution );
    }

    void TPhysicsSystem::resolveDynamicVsDynamic( TBodyEntry& p_a, const TColliderComponent& p_colA, const glm::vec3& p_scaleA, const glm::quat& p_rotA,
                                                  TBodyEntry& p_b, const TColliderComponent& p_colB, const glm::vec3& p_scaleB, const glm::quat& p_rotB )
    {
        TContact contact{};
        if ( !computeContact( p_a.m_position, p_rotA, p_scaleA, p_colA, p_b.m_position, p_rotB, p_scaleB, p_colB, contact ) ) return;

        TRigidBodyComponent& bodyA  = *p_a.m_body;
        TRigidBodyComponent& bodyB  = *p_b.m_body;
        const float          invSum = bodyA.m_invMass + bodyB.m_invMass;
        if ( invSum <= g_kEpsilon ) return;

        const float shareA = bodyA.m_invMass / invSum;
        const float shareB = bodyB.m_invMass / invSum;
        p_a.m_position += contact.m_normal * ( contact.m_penetration * shareA );
        p_b.m_position -= contact.m_normal * ( contact.m_penetration * shareB );

        const glm::vec3 relVel = bodyA.m_linearVelocity - bodyB.m_linearVelocity;
        const float     vn     = glm::dot( relVel, contact.m_normal );
        if ( vn >= 0.0f ) return;

        float e = std::min( bodyA.m_restitution, bodyB.m_restitution );
        if ( -vn < g_kRestNormalSpeed ) e = 0.0f;

        const float     j       = -( 1.0f + e ) * vn / invSum;
        const glm::vec3 impulse = j * contact.m_normal;
        bodyA.m_linearVelocity += impulse * bodyA.m_invMass;
        bodyB.m_linearVelocity -= impulse * bodyB.m_invMass;
    }

    void TPhysicsSystem::emitOverlap( TColliderComponent& p_a, TSceneNode& p_nodeA, TColliderComponent& p_b, TSceneNode& p_nodeB, TOverlapPhase p_phase )
    {
        ++m_lastTriggerEventCount;
        if ( p_a.m_onOverlap ) p_a.m_onOverlap( p_nodeA, p_nodeB, p_phase );
        if ( p_b.m_onOverlap ) p_b.m_onOverlap( p_nodeB, p_nodeA, p_phase );
        if ( m_overlapListener ) m_overlapListener( p_nodeA, p_nodeB, p_phase );
    }

    void TPhysicsSystem::finishOverlapFrame( const std::unordered_set<TOverlapKey, TOverlapKeyHash>& p_current )
    {
        for ( const TOverlapKey& key : m_activeOverlaps )
        {
            if ( p_current.contains( key ) ) continue;
            auto aIt = m_colliders.find( key.m_a );
            auto bIt = m_colliders.find( key.m_b );
            if ( aIt == m_colliders.end() || bIt == m_colliders.end() ) continue;
            emitOverlap( *aIt->second.m_collider, *aIt->second.m_node, *bIt->second.m_collider, *bIt->second.m_node, TOverlapPhase::Exit );
        }

        for ( const TOverlapKey& key : p_current )
        {
            auto aIt = m_colliders.find( key.m_a );
            auto bIt = m_colliders.find( key.m_b );
            if ( aIt == m_colliders.end() || bIt == m_colliders.end() ) continue;

            const TOverlapPhase phase = m_activeOverlaps.contains( key ) ? TOverlapPhase::Stay : TOverlapPhase::Enter;
            emitOverlap( *aIt->second.m_collider, *aIt->second.m_node, *bIt->second.m_collider, *bIt->second.m_node, phase );
        }

        m_activeOverlaps = p_current;
    }

    void TPhysicsSystem::collideAndResolve()
    {
        TOMOS_HEAP_PROBE( "physics.collideAndResolve" );
        m_lastContactCount = 0;

        struct TCollCached
        {
            TColliderEntry* m_entry     = nullptr;
            TBodyEntry*     m_bodyEntry = nullptr;
            TWorldPose      m_pose{};
            TBroadAabb      m_box{};
            bool            m_dynamic = false;
        };

        TFrameAllocator& frame = TFrameAllocator::get();
        TArena&          arena = frame.arena();

        TArenaVector<TCollCached> colliders{ TArenaAllocator<TCollCached>( arena ) };
        colliders.reserve( m_colliders.size() );

        for ( auto& [ col, entry ] : m_colliders )
        {
            ( void ) col;
            if ( entry.m_collider == nullptr || !entry.m_collider->m_enabled ) continue;

            TCollCached c{};
            c.m_entry = &entry;
            c.m_pose  = worldPose( *entry.m_node );

            TRigidBodyComponent* body = bodyOn( entry.m_node );
            if ( body != nullptr && body->isDynamic() )
            {
                auto it = m_bodies.find( body );
                if ( it == m_bodies.end() ) continue;
                c.m_bodyEntry     = &it->second;
                c.m_dynamic       = true;
                c.m_pose.m_center = it->second.m_position;
            }

            {
                const TWorldAABB w = worldAabb( c.m_pose, *entry.m_collider );
                c.m_box            = { w.m_min, w.m_max };
            }
            colliders.push_back( c );
        }

        const float cellSize = std::max( m_broadphaseCellSize, 0.1f );
        auto cellCoord = [ cellSize ]( float p_v ) { return static_cast<int>( std::floor( static_cast<double>( p_v ) / static_cast<double>( cellSize ) ) ); };
        auto cellKey   = []( int p_x, int p_y, int p_z ) -> uint64_t
        {
            const auto ux = static_cast<uint32_t>( p_x );
            const auto uy = static_cast<uint32_t>( p_y );
            const auto uz = static_cast<uint32_t>( p_z );
            return ( static_cast<uint64_t>( ux ) * 73856093ull ) ^ ( static_cast<uint64_t>( uy ) * 19349663ull ) ^
                   ( static_cast<uint64_t>( uz ) * 83492791ull );
        };

        m_scratchCells.clear();
        m_scratchCells.reserve( colliders.size() * 2 );

        for ( int i = 0; i < static_cast<int>( colliders.size() ); ++i )
        {
            const TBroadAabb& box = colliders[ static_cast<size_t>( i ) ].m_box;
            const int         x0  = cellCoord( box.m_min.x );
            const int         y0  = cellCoord( box.m_min.y );
            const int         z0  = cellCoord( box.m_min.z );
            const int         x1  = cellCoord( box.m_max.x );
            const int         y1  = cellCoord( box.m_max.y );
            const int         z1  = cellCoord( box.m_max.z );
            for ( int x = x0; x <= x1; ++x )
                for ( int y = y0; y <= y1; ++y )
                    for ( int z = z0; z <= z1; ++z ) m_scratchCells[ cellKey( x, y, z ) ].push_back( i );
        }

        m_scratchPairKeys.clear();
        m_scratchPairKeys.reserve( colliders.size() * 4 );
        auto packPair = []( int p_a, int p_b ) -> uint64_t
        {
            if ( p_a > p_b ) std::swap( p_a, p_b );
            return ( static_cast<uint64_t>( static_cast<uint32_t>( p_a ) ) << 32 ) | static_cast<uint32_t>( p_b );
        };

        for ( const auto& [ key, indices ] : m_scratchCells )
        {
            ( void ) key;
            for ( size_t ii = 0; ii < indices.size(); ++ii )
            {
                for ( size_t jj = ii + 1; jj < indices.size(); ++jj ) m_scratchPairKeys.insert( packPair( indices[ ii ], indices[ jj ] ) );
            }
        }

        using TPair = std::pair<int, int>;
        TArenaVector<TPair> pairs{ TArenaAllocator<TPair>( arena ) };
        pairs.reserve( m_scratchPairKeys.size() );
        for ( uint64_t packed : m_scratchPairKeys )
        {
            const int i = static_cast<int>( packed >> 32 );
            const int j = static_cast<int>( packed & 0xffffffffu );
            pairs.emplace_back( i, j );
        }
        std::sort( pairs.begin(), pairs.end() );

        m_scratchOverlaps.clear();

        for ( const auto& [ i, j ] : pairs )
        {
            TCollCached& a = colliders[ static_cast<size_t>( i ) ];
            TCollCached& b = colliders[ static_cast<size_t>( j ) ];
            if ( a.m_entry->m_node == b.m_entry->m_node ) continue;
            if ( !aabbOverlap( a.m_box, b.m_box ) ) continue;

            TColliderComponent& colA = *a.m_entry->m_collider;
            TColliderComponent& colB = *b.m_entry->m_collider;
            if ( !colA.interactsWith( colB ) ) continue;

            TContact contact{};
            if ( !computeContact( a.m_pose.m_center, a.m_pose.m_rotation, a.m_pose.m_scale, colA, b.m_pose.m_center, b.m_pose.m_rotation, b.m_pose.m_scale,
                                  colB, contact ) )
                continue;

            const bool triggerPair = colA.m_isTrigger || colB.m_isTrigger;
            if ( triggerPair )
            {
                m_scratchOverlaps.insert( makeOverlapKey( &colA, &colB ) );
                continue;
            }

            if ( a.m_dynamic && b.m_dynamic )
            {
                resolveDynamicVsDynamic( *a.m_bodyEntry, colA, a.m_pose.m_scale, a.m_pose.m_rotation, *b.m_bodyEntry, colB, b.m_pose.m_scale,
                                         b.m_pose.m_rotation );
                a.m_pose.m_center = a.m_bodyEntry->m_position;
                b.m_pose.m_center = b.m_bodyEntry->m_position;
                {
                    const TWorldAABB wa = worldAabb( a.m_pose, colA );
                    const TWorldAABB wb = worldAabb( b.m_pose, colB );
                    a.m_box             = { wa.m_min, wa.m_max };
                    b.m_box             = { wb.m_min, wb.m_max };
                }
                ++m_lastContactCount;
            }
            else if ( a.m_dynamic && !b.m_dynamic )
            {
                resolveDynamicVsStatic( *a.m_bodyEntry, colA, a.m_pose.m_scale, a.m_pose.m_rotation, b.m_pose, colB );
                a.m_pose.m_center = a.m_bodyEntry->m_position;
                {
                    const TWorldAABB wa = worldAabb( a.m_pose, colA );
                    a.m_box             = { wa.m_min, wa.m_max };
                }
                ++m_lastContactCount;
            }
            else if ( b.m_dynamic && !a.m_dynamic )
            {
                resolveDynamicVsStatic( *b.m_bodyEntry, colB, b.m_pose.m_scale, b.m_pose.m_rotation, a.m_pose, colA );
                b.m_pose.m_center = b.m_bodyEntry->m_position;
                {
                    const TWorldAABB wb = worldAabb( b.m_pose, colB );
                    b.m_box             = { wb.m_min, wb.m_max };
                }
                ++m_lastContactCount;
            }
        }

        finishOverlapFrame( m_scratchOverlaps );
    }

    void TPhysicsSystem::writeTransforms()
    {
        // Blend previous → current by leftover accumulator fraction so the
        // rendered pose tracks wall-clock time between fixed steps.
        for ( auto& [ bodyPtr, entry ] : m_bodies )
        {
            ( void ) bodyPtr;
            if ( !entry.m_body->isDynamic() ) continue;
            entry.m_lastWritten = glm::mix( entry.m_previousPosition, entry.m_position, m_alpha );
            setWorldTranslation( *entry.m_node, entry.m_lastWritten );
        }
    }

    void TPhysicsSystem::forEachBody( const std::function<void( const TBodyDebug& )>& p_fn ) const
    {
        for ( const auto& [ bodyPtr, entry ] : m_bodies )
        {
            ( void ) bodyPtr;
            TBodyDebug d{};
            d.m_node        = entry.m_node;
            d.m_body        = entry.m_body;
            d.m_simPosition = entry.m_position;
            p_fn( d );
        }
    }

    void TPhysicsSystem::forEachCollider( const std::function<void( const TColliderDebug& )>& p_fn ) const
    {
        for ( const auto& [ colPtr, entry ] : m_colliders )
        {
            ( void ) colPtr;
            TColliderDebug d{};
            d.m_node     = entry.m_node;
            d.m_collider = entry.m_collider;
            p_fn( d );
        }
    }

    bool TPhysicsSystem::colliderWorldAabb( const TSceneNode& p_node, const TColliderComponent& p_col, glm::vec3& p_min, glm::vec3& p_max )
    {
        if ( !p_col.m_enabled ) return false;
        const TWorldPose pose = worldPose( p_node );
        const TWorldAABB box  = worldAabb( pose, p_col );
        p_min                 = box.m_min;
        p_max                 = box.m_max;
        return true;
    }
}  // namespace Tomos
