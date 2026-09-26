#include "Tomos/systems/texture/TAnimatedTextureSystem.hh"

#include <unordered_set>

#include "Tomos/core/scene/TScene.hh"
#include "Tomos/core/scene/TSceneResourceBag.hh"
#include "Tomos/gpu/vulkan/TVkGpu.hh"
#include "Tomos/systems/mesh/TMeshSystem.hh"
#include "Tomos/systems/particle/TParticleSystem.hh"
#include "Tomos/systems/sprite/TSpriteSystem.hh"

namespace Tomos
{
    void TAnimatedTextureSystem::update( float p_dt )
    {
        if ( m_gpu == nullptr || m_scene == nullptr ) return;

        TSceneResourceBag& bag = m_scene->resources();
        TECS&              ecs = m_scene->ecs();

        std::unordered_set<const TVkImage*> inUse;

        if ( auto* sprites = ecs.maybeGetSystem<TSpriteSystem>() )
        {
            for ( const auto& [ sc, node ] : sprites->m_sprites )
            {
                ( void ) node;
                if ( sc->m_visible && sc->m_texture != nullptr ) inUse.insert( sc->m_texture );
            }
        }

        if ( auto* particles = ecs.maybeGetSystem<TParticleSystem>() )
        {
            for ( const auto& [ emitter, node ] : particles->m_emitters )
            {
                ( void ) node;
                if ( emitter->m_emitting && emitter->m_texture != nullptr ) inUse.insert( emitter->m_texture );
            }
        }

        if ( auto* meshes = ecs.maybeGetSystem<TMeshSystem>() )
        {
            for ( const auto& [ mc, node ] : meshes->m_meshes )
            {
                ( void ) node;
                if ( mc->m_baseOverrideImage != nullptr ) inUse.insert( mc->m_baseOverrideImage );
                if ( mc->m_emissionOverrideImage != nullptr ) inUse.insert( mc->m_emissionOverrideImage );
            }
        }

        bag.tickAnimatedTextures( *m_gpu, p_dt, inUse );
    }
}  // namespace Tomos
