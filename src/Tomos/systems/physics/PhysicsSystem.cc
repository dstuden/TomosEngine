//
// Created by dstuden on 4/10/25.
//

#include "PhysicsSystem.hh"

namespace Tomos
{
    /*
     * Override the pixed time step system update since Jolt has its own
     * ignore p_deltaTime
     */
    void PhysicsSystem::update( float p_deltaTime, const std::string& p_layerId )
    {
        m_physicsSystem.Update( m_fixedTimeStep, m_maxPhysicsSteps, m_tempAllocator.get(), m_jobSystem.get() );

        earlyUpdate( p_layerId );
        update( p_layerId );
        lateUpdate( p_layerId );
    }
} // Tomos
