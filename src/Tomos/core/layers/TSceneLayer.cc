#include "Tomos/core/layers/TSceneLayer.hh"

#include "Tomos/core/app/TApplication.hh"
#include "Tomos/gpu/vulkan/TVkGpu.hh"
#include "Tomos/systems/animation/TAnimationSystem.hh"
#include "Tomos/systems/audio/TAudioSystem.hh"
#include "Tomos/systems/camera/TCameraSystem.hh"
#include "Tomos/systems/light/TLightSystem.hh"
#include "Tomos/systems/mesh/TMeshSystem.hh"
#include "Tomos/systems/particle/TParticleSystem.hh"
#include "Tomos/systems/physics/TPhysicsSystem.hh"
#include "Tomos/systems/script/TScriptSystem.hh"
#include "Tomos/systems/sprite/TSpriteSystem.hh"
#include "Tomos/systems/texture/TAnimatedTextureSystem.hh"
#include "Tomos/util/profile/TProfile.hh"

namespace Tomos
{
    TSceneLayer::TSceneLayer( std::string p_name ) : TLayer( std::move( p_name ) ) {}

    TScene& TSceneLayer::scene() { return TApplication::get().sceneManager().scene(); }

    void TSceneLayer::registerDefaultSystems( TScene& p_scene )
    {
        auto& ecs = p_scene.ecs();
        if ( ecs.maybeGetSystem<TScriptSystem>() == nullptr ) ecs.addSystem( std::make_unique<TScriptSystem>() );
        // Physics after scripts so gameplay can apply forces in the same frame.
        if ( ecs.maybeGetSystem<TPhysicsSystem>() == nullptr )
        {
            auto phys = std::make_unique<TPhysicsSystem>();
            phys->setTime( &TApplication::get().time() );
            ecs.addSystem( std::move( phys ) );
        }
        if ( ecs.maybeGetSystem<TAnimationSystem>() == nullptr ) ecs.addSystem( std::make_unique<TAnimationSystem>() );
        if ( ecs.maybeGetSystem<TAnimatedTextureSystem>() == nullptr ) ecs.addSystem( std::make_unique<TAnimatedTextureSystem>() );
        if ( ecs.maybeGetSystem<TCameraSystem>() == nullptr ) ecs.addSystem( std::make_unique<TCameraSystem>() );
        if ( ecs.maybeGetSystem<TMeshSystem>() == nullptr ) ecs.addSystem( std::make_unique<TMeshSystem>() );
        if ( ecs.maybeGetSystem<TLightSystem>() == nullptr ) ecs.addSystem( std::make_unique<TLightSystem>() );
        if ( ecs.maybeGetSystem<TSpriteSystem>() == nullptr ) ecs.addSystem( std::make_unique<TSpriteSystem>() );
        if ( ecs.maybeGetSystem<TParticleSystem>() == nullptr ) ecs.addSystem( std::make_unique<TParticleSystem>() );
        if ( ecs.maybeGetSystem<TAudioSystem>() == nullptr ) ecs.addSystem( std::make_unique<TAudioSystem>() );

        TVkGpu* gpu = TApplication::get().gpu();
        if ( auto* mesh = ecs.maybeGetSystem<TMeshSystem>() )
        {
            mesh->setScene( &p_scene );
            mesh->setGpu( gpu );
        }
        if ( auto* animTex = ecs.maybeGetSystem<TAnimatedTextureSystem>() )
        {
            animTex->setScene( &p_scene );
            animTex->setGpu( gpu );
        }
        if ( auto* audio = ecs.maybeGetSystem<TAudioSystem>() )
        {
            if ( auto* cameras = ecs.maybeGetSystem<TCameraSystem>() ) audio->setCameraSystem( cameras );
        }
    }

    void TSceneLayer::onAttach()
    {
        registerDefaultSystems( scene() );
        scene().activate();
    }

    void TSceneLayer::onDetach()
    {
        if ( auto scene = TApplication::get().sceneManager().scenePtr() ) scene->deactivate();
    }

    void TSceneLayer::onUpdate( float p_dt )
    {
        auto& sc  = scene();
        auto& ecs = sc.ecs();

        const bool simulating = TApplication::get().sceneManager().isSimulationPlaying();

        // ECS: earlyUpdate → update → computeTransforms → lateUpdate.
        // Populate is deferred to onRender so post-update TRS mutations
        // (overlays / tooling) are visible in the same frame.
        if ( simulating )
        {
            ecs.earlyUpdate( p_dt );
            ecs.update( p_dt );
        }
        sc.computeTransforms();
        if ( simulating ) ecs.lateUpdate( p_dt );

        m_lastDt = p_dt;
    }

    void TSceneLayer::onRender()
    {
        TVkGpu* gpu = TApplication::get().gpu();
        if ( gpu == nullptr ) return;

        auto& sc  = scene();
        auto& ecs = sc.ecs();

        // Pick up TRS edits from overlays that ran after onUpdate.
        sc.computeTransforms();

        auto&       state      = gpu->frameState();
        const float aspect     = gpu->renderAspectRatio();
        const bool  simulating = TApplication::get().sceneManager().isSimulationPlaying();
        state.m_time           = TApplication::get().time().elapsed();
        {
            TOMOS_PROFILE_SCOPE( "Populate.Camera" );
            ecs.getSystem<TCameraSystem>().populate( state, aspect );
        }
        {
            TOMOS_PROFILE_SCOPE( "Populate.Mesh" );
            ecs.getSystem<TMeshSystem>().populate( state );
        }
        {
            TOMOS_PROFILE_SCOPE( "Populate.Light" );
            ecs.getSystem<TLightSystem>().populate( state );
        }
        {
            TOMOS_PROFILE_SCOPE( "Populate.Sprite" );
            ecs.getSystem<TSpriteSystem>().populate( state );
        }
        {
            TOMOS_PROFILE_SCOPE( "Populate.Particle" );
            // Particles still advance when paused would freeze GPU sim oddly — skip dt.
            ecs.getSystem<TParticleSystem>().populate( state, simulating ? m_lastDt : 0.0f );
        }

        gpu->render();
    }

    void TSceneLayer::onEvent( TEvent& /*p_event*/ ) {}
}  // namespace Tomos
