//
// Created by dstuden on 4/10/25.
//

#include "TPhysicsSystem.hh"

namespace Tomos
{
    /*
     * Override the pixed time step system update since Jolt has its own
     * ignore p_deltaTime
     */
    void TPhysicsSystem::update( float p_deltaTime, const std::string& p_layerId )
    {
        earlyUpdate( p_layerId );
        update( p_layerId );
        lateUpdate( p_layerId );
    }
} // Tomos
