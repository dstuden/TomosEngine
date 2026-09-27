// Vulkan + GLFW must be included first so Vulkan headers are found by GLFW.
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <vector>

#include "Tomos/core/app/TApplication.hh"
#include "Tomos/core/input/TActionMap.hh"
#include "Tomos/core/input/TInput.hh"
#include "Tomos/core/layers/TSceneLayer.hh"
#include "Tomos/core/scene/TScene.hh"
#include "Tomos/core/scene/TSceneNode.hh"
#include "Tomos/core/window/TWindow.hh"
#include "Tomos/gpu/vulkan/TMeshTechnique.hh"
#include "Tomos/gpu/vulkan/TVkGpu.hh"
#include "Tomos/gpu/vulkan/TVkMaterial.hh"
#include "Tomos/systems/animation/TAnimatorComponent.hh"
#include "Tomos/systems/asset/TAssetSystem.hh"
#include "Tomos/systems/audio/TAudioComponent.hh"
#include "Tomos/systems/audio/TAudioSystem.hh"
#include "Tomos/systems/camera/TCameraComponent.hh"
#include "Tomos/systems/light/TLightComponent.hh"
#include "Tomos/systems/mesh/TMeshComponent.hh"
#include "Tomos/systems/particle/TParticleEmitterComponent.hh"
#include "Tomos/systems/physics/TColliderComponent.hh"
#include "Tomos/systems/physics/TPhysicsLayers.hh"
#include "Tomos/systems/physics/TRigidBodyComponent.hh"
#include "Tomos/systems/script/TScript.hh"
#include "Tomos/systems/script/TScriptComponent.hh"
#include "Tomos/systems/script/TScriptRegistry.hh"
#include "Tomos/systems/sprite/TSpriteComponent.hh"
#include "Tomos/ui/TImGuiBackend.hh"
#ifdef TOMOS_EDITOR
#include "Tomos/ui/editor/TSceneEditorLayer.hh"
#endif
#include "Tomos/systems/asset/TAssetLoadQueue.hh"
#include "Tomos/systems/mesh/TMeshPrimitives.hh"
#include "Tomos/util/logger/TLogger.hh"

using namespace Tomos;

// Sandbox gameplay hooks (F = beep). Refreshed after procedural spawn / scene load.
static TAudioComponent* g_beepEmitter = nullptr;

namespace
{
    TSceneNode* findNamedNode( TSceneNode& p_root, const std::string& p_name ) { return p_root.findByName( p_name ); }

    const TAnimationClip* findClipQuiet( const TGpuAsset& p_asset, const std::string& p_name )
    {
        for ( const auto& c : p_asset.m_clips )
            if ( c->m_name == p_name ) return c.get();
        return nullptr;
    }

    // CesiumMan ships one clip — synthesize Idle (hold first pose) and wire Idle↔Walk.
    void setupIdleWalkBlend( TAnimatorComponent& p_anim, TGpuAsset& p_asset )
    {
        if ( p_asset.m_clips.empty() ) return;

        TAnimationClip* walk = p_asset.m_clips.front().get();
        if ( walk->m_name.empty() || walk->m_name == "None" ) walk->m_name = "Walk";

        const TAnimationClip* idle = findClipQuiet( p_asset, "Idle" );
        if ( idle == nullptr )
        {
            p_asset.m_clips.push_back( std::make_unique<TAnimationClip>( makeHoldPoseClip( *walk, "Idle", 1.0f ) ) );
            idle = p_asset.m_clips.back().get();
        }

        p_anim.m_defaultFadeDuration = 0.35f;
        p_anim.m_stateMachine.clear();
        p_anim.m_stateMachine.addState( "Idle", idle );
        p_anim.m_stateMachine.addState( "Walk", walk );
        p_anim.m_stateMachine.addTransition( "Idle", "Walk", "Moving", true, 0.35f );
        p_anim.m_stateMachine.addTransition( "Walk", "Idle", "Moving", false, 0.35f );
        p_anim.m_stateMachine.setBool( "Moving", false );
        p_anim.m_stateMachine.start( "Idle" );
        if ( const TAnimState* idleState = p_anim.m_stateMachine.findState( "Idle" ) ) p_anim.applyState( *idleState, 0.0f, true );
    }
}  // namespace

// Toggles CesiumMan Idle↔Walk via state-machine bool "Moving" (auto + G key).
class AnimBlendDemoScript : public TScript
{
public:
    float m_period = 2.5f;

    [[nodiscard]] std::string_view typeName() const override { return "AnimBlendDemo"; }

private:
    float m_timer  = 0.0f;
    bool  m_moving = false;
    bool  m_gDown  = false;

    void update( float p_dt ) override
    {
        auto* anim = node().findComponent<TAnimatorComponent>();
        if ( anim == nullptr || !anim->m_stateMachine.m_enabled ) return;

        GLFWwindow*           win   = TApplication::get().window().getNativeWindow();
        const TInput::BlockFn block = [] { return TImGuiBackend::ioBlocksGameInput(); };

        bool toggled = false;
        if ( TInput::keyPressed( win, GLFW_KEY_G, m_gDown, block ) )
        {
            m_moving = !m_moving;
            m_timer  = 0.0f;
            toggled  = true;
        }

        m_timer += p_dt;
        if ( m_timer >= m_period )
        {
            m_timer  = 0.0f;
            m_moving = !m_moving;
            toggled  = true;
        }

        if ( toggled )
        {
            anim->m_stateMachine.setBool( "Moving", m_moving );
            TLOG_INFO() << "[Sandbox] Anim blend → " << ( m_moving ? "Walk" : "Idle" );
        }
    }
};

class FlyCameraScript : public TScript
{
public:
    float m_moveSpeed    = 5.0f;
    float m_lookSens     = 0.002f;
    float m_padLookSpeed = 2.5f;  // rad/s at full stick deflection

    [[nodiscard]] std::string_view typeName() const override { return "FlyCamera"; }

private:
    float m_yaw      = 0.0f;
    float m_pitch    = 0.0f;
    bool  m_captured = true;
    bool  m_tabDown  = false;
    bool  m_beepDown = false;

    TActionMap m_actions;

    void onAttach() override
    {
        buildActionMap();
        TApplication::get().window().setCursorMode( TWindow::TCursorMode::Disabled );
    }

    void onDetach() override { TApplication::get().window().setCursorMode( TWindow::TCursorMode::Normal ); }

    void buildActionMap()
    {
        m_actions.clear();

        m_actions.bindKeyAxis( "MoveForward", GLFW_KEY_S, GLFW_KEY_W );
        m_actions.bindGamepadAxis( "MoveForward", GLFW_GAMEPAD_AXIS_LEFT_Y, -1.0f );

        m_actions.bindKeyAxis( "MoveRight", GLFW_KEY_A, GLFW_KEY_D );
        m_actions.bindGamepadAxis( "MoveRight", GLFW_GAMEPAD_AXIS_LEFT_X );

        m_actions.bindKeyAxis( "MoveUp", GLFW_KEY_LEFT_CONTROL, GLFW_KEY_SPACE );
        m_actions.bindGamepadButton( "MoveUp", GLFW_GAMEPAD_BUTTON_RIGHT_BUMPER, 0, TInputContext::Gameplay, 1.0f );
        m_actions.bindGamepadButton( "MoveUp", GLFW_GAMEPAD_BUTTON_LEFT_BUMPER, 0, TInputContext::Gameplay, -1.0f );

        m_actions.bindKey( "Sprint", GLFW_KEY_LEFT_SHIFT );
        m_actions.bindGamepadButton( "Sprint", GLFW_GAMEPAD_BUTTON_LEFT_THUMB );

        m_actions.bindGamepadAxis( "LookX", GLFW_GAMEPAD_AXIS_RIGHT_X );
        m_actions.bindGamepadAxis( "LookY", GLFW_GAMEPAD_AXIS_RIGHT_Y, -1.0f );

        m_actions.bindKey( "Beep", GLFW_KEY_F );
        m_actions.bindGamepadButton( "Beep", GLFW_GAMEPAD_BUTTON_X );

        m_actions.bindKey( "ToggleCapture", GLFW_KEY_TAB, TInputContext::Any );
        m_actions.bindKey( "Quit", GLFW_KEY_ESCAPE, TInputContext::Any );
        m_actions.bindGamepadButton( "Quit", GLFW_GAMEPAD_BUTTON_START, 0, TInputContext::Any );
    }

    void update( float p_dt ) override
    {
        GLFWwindow* win = TApplication::get().window().getNativeWindow();

        m_actions.setActiveContext( m_captured ? TInputContext::Gameplay : TInputContext::UI );

        // Fly-cam owns input while captured; otherwise defer to the debug UI.
        const TInput::BlockFn blockGame = [ this ] { return !m_captured && TImGuiBackend::ioBlocksGameInput(); };
        const TInput::BlockFn blockText = [] { return TImGuiBackend::ioWantTextInput(); };

        if ( m_actions.pressed( "ToggleCapture", m_tabDown, win, blockText ) )
        {
            m_captured = !m_captured;
            TApplication::get().window().setCursorMode( m_captured ? TWindow::TCursorMode::Disabled : TWindow::TCursorMode::Normal );
            // Drop the next mouse delta so look doesn't jump after re-capture.
            TInput::discardMouseDelta( win );
        }

        double dx = 0.0, dy = 0.0;
        TInput::mouseDelta( dx, dy );

        float lookX = m_actions.value( "LookX", win, blockGame );
        float lookY = m_actions.value( "LookY", win, blockGame );

        if ( m_captured )
        {
            m_yaw -= static_cast<float>( dx ) * m_lookSens;
            m_pitch -= static_cast<float>( dy ) * m_lookSens;
        }
        m_yaw -= lookX * m_padLookSpeed * p_dt;
        m_pitch -= lookY * m_padLookSpeed * p_dt;
        m_pitch = glm::clamp( m_pitch, glm::radians( -89.0f ), glm::radians( 89.0f ) );

        const glm::quat orientation = glm::angleAxis( m_yaw, glm::vec3( 0.0f, 1.0f, 0.0f ) ) * glm::angleAxis( m_pitch, glm::vec3( 1.0f, 0.0f, 0.0f ) );

        node().m_transform.setRotation( orientation );

        const glm::vec3 forward = orientation * glm::vec3( 0.0f, 0.0f, -1.0f );
        const glm::vec3 right   = orientation * glm::vec3( 1.0f, 0.0f, 0.0f );
        const glm::vec3 worldUp = glm::vec3( 0.0f, 1.0f, 0.0f );

        const float moveF = m_actions.value( "MoveForward", win, blockGame );
        const float moveR = m_actions.value( "MoveRight", win, blockGame );
        const float moveU = m_actions.value( "MoveUp", win, blockGame );

        glm::vec3 move = forward * moveF + right * moveR + worldUp * moveU;

        const float boost = m_actions.down( "Sprint", win, blockGame ) ? 4.0f : 1.0f;

        if ( glm::length( move ) > 0.001f ) node().m_transform.translate( glm::normalize( move ) * m_moveSpeed * boost * p_dt );

        if ( m_actions.pressed( "Beep", m_beepDown, win, blockGame ) && g_beepEmitter != nullptr ) g_beepEmitter->play( true );

        if ( m_actions.down( "Quit", win, blockGame ) ) TApplication::get().close();
    }
};

class SandboxLayer : public TSceneLayer
{
public:
    static constexpr const char* g_kScenePath = "assets/scenes/sandbox.json";

    explicit SandboxLayer( std::string p_modelPath ) : TSceneLayer( "Sandbox" ), m_modelPath( std::move( p_modelPath ) ) {}

    // Refresh Sandbox globals / non-serialized setup after procedural spawn.
    void bindGameplayHooks()
    {
        g_beepEmitter = nullptr;

        if ( TSceneNode* cam = findNamedNode( scene(), "Camera" ) )
        {
            if ( cam->findComponent<TScriptComponent>() == nullptr ) cam->addComponent( TScriptComponent::make<FlyCameraScript>() );
        }

        if ( TSceneNode* beep = findNamedNode( scene(), "BeepEmitter" ) ) g_beepEmitter = beep->findComponent<TAudioComponent>();

        if ( TSceneNode* man = findNamedNode( scene(), "CesiumManRoot" ) )
        {
            if ( auto* anim = man->findComponent<TAnimatorComponent>() )
            {
                if ( TGpuAsset* asset = TApplication::get().assetSystem().maybeGetAsset( "CesiumMan" ) ) setupIdleWalkBlend( *anim, *asset );
            }
            if ( man->findComponent<TScriptComponent>() == nullptr ) man->addComponent( TScriptComponent::make<AnimBlendDemoScript>() );
        }
    }

    void onDetach() override
    {
        g_beepEmitter = nullptr;
        TSceneLayer::onDetach();
    }

    void onAttach() override
    {
        TSceneLayer::onAttach();
        spawnDefaultScene();
        bindGameplayHooks();
    }

private:
    // Fixed-timestep smoke test: static floor + two balls + trigger zone.
    void spawnPhysicsDemo()
    {
        auto& app = TApplication::get();
        auto* gpu = app.gpu();
        if ( gpu == nullptr ) return;

        TGpuAsset* primAsset = app.assetSystem().maybeGetAsset( "PhysicsPrimitives" );
        if ( primAsset == nullptr )
        {
            auto asset    = std::make_unique<TGpuAsset>();
            asset->m_name = "PhysicsPrimitives";

            asset->m_meshes.push_back( TMeshPrimitives::makeBox( *gpu ) );
            asset->m_meshes.push_back( TMeshPrimitives::makeUVSphere( *gpu ) );

            auto fillMaterial = [ & ]( TVkMaterialDesc& p_desc )
            {
                p_desc.m_baseTexture     = &gpu->defaultTexture();
                p_desc.m_metRghTexture   = &gpu->defaultTexture();
                p_desc.m_emissionTexture = &gpu->defaultTexture();
                p_desc.m_normalTexture   = &gpu->defaultTexture();
            };

            {
                TVkMaterialDesc desc{};
                desc.m_baseColorFactor = { 0.28f, 0.34f, 0.48f, 1.0f };
                desc.m_metallicFactor  = 0.0f;
                desc.m_roughnessFactor = 0.85f;
                fillMaterial( desc );
                asset->m_materials.push_back(
                        std::make_unique<TVkMaterial>( gpu->device(), gpu->physDevice(), gpu->descPool(), gpu->layouts().m_material, desc ) );
            }
            {
                TVkMaterialDesc desc{};
                desc.m_baseColorFactor = { 1.0f, 0.35f, 0.15f, 1.0f };
                desc.m_metallicFactor  = 0.05f;
                desc.m_roughnessFactor = 0.4f;
                fillMaterial( desc );
                asset->m_materials.push_back(
                        std::make_unique<TVkMaterial>( gpu->device(), gpu->physDevice(), gpu->descPool(), gpu->layouts().m_material, desc ) );
            }

            app.assetSystem().registerAsset( std::move( asset ) );
            primAsset = app.assetSystem().maybeGetAsset( "PhysicsPrimitives" );
        }
        if ( primAsset == nullptr || primAsset->m_meshes.size() < 2 || primAsset->m_materials.size() < 2 ) return;

        const TVkMesh*     boxMesh    = primAsset->m_meshes[ 0 ].get();
        const TVkMesh*     sphereMesh = primAsset->m_meshes[ 1 ].get();
        const TVkMaterial* floorMat   = primAsset->m_materials[ 0 ].get();
        const TVkMaterial* ballMat    = primAsset->m_materials[ 1 ].get();

        {
            constexpr glm::vec3 size{ 4.0f, 0.3f, 4.0f };
            TSceneNode&         floor = scene().createNode( "PhysicsFloor" );
            floor.m_transform.setTranslation( { -1.5f, size.y * 0.5f, 1.5f } );
            floor.m_transform.setScale( size );

            auto& col         = floor.emplaceComponent<TColliderComponent>();
            col.m_shape       = TColliderShape::Box;
            col.m_halfExtents = { 0.5f, 0.5f, 0.5f };
            col.m_layer       = TPhysicsLayer::g_static;
            col.m_mask        = TPhysicsLayer::g_dynamic;
            floor.emplaceComponent<TMeshComponent>( boxMesh, floorMat, true );

            scene().addChild( &floor );
        }

        auto spawnPhysicsBall = [ & ]( const char* p_name, const glm::vec3& p_pos, float p_restitution, const glm::vec3& p_velocity )
        {
            constexpr float radius = 0.25f;
            TSceneNode&     ball   = scene().createNode( p_name );
            ball.m_transform.setTranslation( p_pos );
            ball.m_transform.setScale( glm::vec3( radius * 2.0f ) );

            auto& body = ball.emplaceComponent<TRigidBodyComponent>();
            body.setMass( 1.0f );
            body.m_restitution    = p_restitution;
            body.m_linearDamping  = 0.02f;
            body.m_linearVelocity = p_velocity;

            auto& col    = ball.emplaceComponent<TColliderComponent>();
            col.m_shape  = TColliderShape::Sphere;
            col.m_radius = 0.5f;
            col.m_layer  = TPhysicsLayer::g_dynamic;
            col.m_mask   = TPhysicsLayer::g_static | TPhysicsLayer::g_dynamic | TPhysicsLayer::g_trigger;
            ball.emplaceComponent<TMeshComponent>( sphereMesh, ballMat, true );

            scene().addChild( &ball );
        };

        spawnPhysicsBall( "PhysicsBall", { -1.5f, 4.0f, 1.5f }, 0.35f, {} );
        spawnPhysicsBall( "PhysicsBallB", { -2.4f, 4.2f, 1.5f }, 0.2f, { 2.5f, 0.0f, 0.0f } );

        {
            TSceneNode& zone = scene().createNode( "PhysicsTriggerZone" );
            zone.m_transform.setTranslation( { -1.5f, 1.4f, 1.5f } );
            zone.m_transform.setScale( { 2.0f, 1.2f, 2.0f } );

            auto& col         = zone.emplaceComponent<TColliderComponent>();
            col.m_shape       = TColliderShape::Box;
            col.m_halfExtents = { 0.5f, 0.5f, 0.5f };
            col.m_isTrigger   = true;
            col.m_layer       = TPhysicsLayer::g_trigger;
            col.m_mask        = TPhysicsLayer::g_dynamic;
            col.m_onOverlap   = []( TSceneNode& /*self*/, TSceneNode& p_other, TOverlapPhase p_phase )
            {
                if ( p_phase == TOverlapPhase::Stay ) return;
                const char* label = ( p_phase == TOverlapPhase::Enter ) ? "enter" : "exit";
                TLOG_INFO() << "[Sandbox] Trigger zone " << label << ": " << p_other.m_name;
            };
            zone.emplaceComponent<TMeshComponent>( boxMesh, floorMat, true );

            scene().addChild( &zone );
        }

        TLOG_INFO() << "[Sandbox] Physics demo: balls drop / collide; trigger zone logs enter/exit";
    }

    void spawnDefaultScene()
    {
        auto& app   = TApplication::get();
        auto& loads = app.assetLoadQueue();

        TLOG_INFO() << "[Sandbox] Loading model: " << m_modelPath;
        const TAssetLoadHandle modelHandle = loads.requestLoad( m_modelPath );
        loads.waitUntilReady( modelHandle, scene() );
        TGpuAsset*  modelAsset = loads.asset( modelHandle );
        TSceneNode* modelRoot  = loads.takeRoot( modelHandle, scene() );
        if ( modelAsset == nullptr || modelRoot == nullptr )
        {
            TLOG_ERROR() << "[Sandbox] Failed to load model: " << m_modelPath << " (" << loads.error( modelHandle ) << ")";
            return;
        }
        TLOG_INFO() << "[Sandbox] Loaded '" << modelAsset->m_name << "' (" << modelAsset->m_meshes.size() << " meshes, " << modelAsset->m_materials.size()
                    << " materials)";
        scene().addChild( modelRoot );

        {
            const TAssetLoadHandle animHandle = loads.requestLoad( "assets/CesiumMan.glb" );
            loads.waitUntilReady( animHandle, scene() );
            TGpuAsset*  animAsset = loads.asset( animHandle );
            TSceneNode* animRoot  = loads.takeRoot( animHandle, scene() );
            if ( animAsset == nullptr || animRoot == nullptr )
            {
                TLOG_ERROR() << "[Sandbox] Failed to load CesiumMan (" << loads.error( animHandle ) << ")";
            }
            else
            {
                TLOG_INFO() << "[Sandbox] Loaded '" << animAsset->m_name << "' (" << animAsset->m_meshes.size() << " meshes, " << animAsset->m_clips.size()
                            << " clips)";

                TSceneNode& wrapper = scene().createNode( "CesiumManRoot" );
                wrapper.m_transform.setTranslation( { 2.0f, 0.0f, 0.0f } );

                auto& animator = wrapper.emplaceComponent<TAnimatorComponent>();
                if ( !animAsset->m_clips.empty() )
                {
                    setupIdleWalkBlend( animator, *animAsset );
                    TLOG_INFO() << "[Sandbox] CesiumMan Idle↔Walk crossfade (auto-toggle / G key)";
                }
                wrapper.addComponent( TScriptComponent::make<AnimBlendDemoScript>() );

                wrapper.addChild( animRoot );
                scene().addChild( &wrapper );
            }
        }

        {
            TAudioClip* beepClip = scene().resources().getOrCreateClip( "assets/beep.wav", "Beep" );
            scene().ecs().getSystem<TAudioSystem>().preload( beepClip );

            TSceneNode& node = scene().createNode( "BeepEmitter" );
            node.m_transform.setTranslation( { 2.0f, 1.5f, 0.0f } );

            auto& audio         = node.emplaceComponent<TAudioComponent>( beepClip );
            audio.m_spatial     = true;
            audio.m_looping     = false;
            audio.m_volume      = 1.0f;
            audio.m_minDistance = 1.0f;
            audio.m_maxDistance = 25.0f;

            auto& sprite   = node.emplaceComponent<TSpriteComponent>();
            sprite.m_size  = { 0.25f, 0.25f };
            sprite.m_color = { 0.2f, 0.9f, 0.4f, 1.0f };
            sprite.m_mode  = TBillboardMode::Spherical;

            scene().addChild( &node );
        }

        {
            TVkImage* sparkTex = scene().resources().loadImage( *app.gpu(), "assets/particle_soft.png" );

            TSceneNode& node = scene().createNode( "SparkEmitter" );
            node.m_transform.setTranslation( { 2.0f, 0.6f, 0.4f } );

            auto& sparks         = node.emplaceComponent<TParticleEmitterComponent>();
            sparks.m_emitting    = true;
            sparks.m_rate        = 80.0f;
            sparks.m_lifetimeMin = 0.35f;
            sparks.m_lifetimeMax = 0.9f;
            sparks.m_velocityMin = { -0.8f, 1.5f, -0.8f };
            sparks.m_velocityMax = { 0.8f, 4.0f, 0.8f };
            sparks.m_sizeStart   = { 0.28f, 0.28f };
            sparks.m_sizeEnd     = { 0.06f, 0.06f };
            sparks.m_colorStart  = { 1.0f, 1.0f, 1.0f, 1.0f };
            sparks.m_colorEnd    = { 1.0f, 0.55f, 0.2f, 0.0f };
            sparks.m_gravity     = -3.5f;
            sparks.m_texture     = sparkTex;

            scene().addChild( &node );
        }

        {
            TSceneNode& node = scene().createNode( "Sun" );
            node.m_transform.setLocalTRS( { 0.0f, 20.0f, 8.0f }, glm::angleAxis( glm::radians( -50.0f ), glm::vec3( 1.0f, 0.0f, 0.0f ) ), glm::vec3( 1.0f ) );

            auto& light        = node.emplaceComponent<TLightComponent>();
            light.m_type       = TLightType::Directional;
            light.m_color      = { 1.0f, 0.9f, 0.75f };
            light.m_intensity  = 3.0f;
            light.m_castShadow = true;
            scene().addChild( &node );
        }

        {
            // Warm fill with cubemap shadows (6 array layers).
            TSceneNode& node = scene().createNode( "FillPoint" );
            node.m_transform.setTranslation( { -1.5f, 2.5f, 0.5f } );

            auto& light        = node.emplaceComponent<TLightComponent>();
            light.m_type       = TLightType::Point;
            light.m_color      = { 1.0f, 0.85f, 0.65f };
            light.m_intensity  = 40.0f;
            light.m_maxRange   = 18.0f;
            light.m_castShadow = true;
            scene().addChild( &node );
        }

        {
            TSceneNode& node = scene().createNode( "Camera" );
            node.m_transform.setTranslation( { 0.0f, 1.8f, 3.0f } );

            auto& cam  = node.emplaceComponent<TCameraComponent>();
            cam.m_fov  = glm::radians( 65.0f );
            cam.m_near = 0.05f;
            cam.m_far  = 500.0f;
            node.addComponent( TScriptComponent::make<FlyCameraScript>() );
            scene().addChild( &node );

            TSceneNode& spotNode = scene().createNode( "CameraSpotlight" );
            spotNode.m_transform.setTranslation( { 0.2f, -0.2f, 0.0f } );

            auto& light        = spotNode.emplaceComponent<TLightComponent>();
            light.m_type       = TLightType::Spot;
            light.m_color      = { 1.0f, 0.95f, 0.8f };
            light.m_intensity  = 25.0f;
            light.m_maxRange   = 20.0f;
            light.m_innerCone  = glm::radians( 12.5f );
            light.m_outerCone  = glm::radians( 17.5f );
            light.m_castShadow = true;

            node.addChild( &spotNode );
        }

        spawnWaterDemo();
        spawnPhysicsDemo();
        spawnAnimatedTextureDemo();
    }

    void spawnAnimatedTextureDemo()
    {
        auto& app = TApplication::get();
        auto* gpu = app.gpu();
        if ( gpu == nullptr ) return;

        {
            TBagAnimatedTextureRef opts{ "assets/textures/demo.gif" };
            TVkImage*              gif = scene().resources().resolveTexture( *gpu, opts.m_path, opts );

            TSceneNode& node = scene().createNode( "AnimGifSprite" );
            node.m_transform.setTranslation( { 1.5f, 1.2f, 1.0f } );

            auto& sprite        = node.emplaceComponent<TSpriteComponent>( gif );
            sprite.m_size       = { 0.6f, 0.6f };
            sprite.m_color      = { 1.0f, 1.0f, 1.0f, 1.0f };
            sprite.m_mode       = TBillboardMode::Spherical;
            sprite.m_animRef    = opts;
            sprite.m_textureRef = TBagTextureRef{ opts.m_path };
            scene().addChild( &node );
        }

        TGpuAsset* screenAsset = app.assetSystem().maybeGetAsset( "VideoScreen" );
        if ( screenAsset == nullptr )
        {
            auto asset    = std::make_unique<TGpuAsset>();
            asset->m_name = "VideoScreen";
            asset->m_meshes.push_back( TMeshPrimitives::makePlane( *gpu ) );

            TVkMaterialDesc desc{};
            desc.m_baseColorFactor = { 1.0f, 1.0f, 1.0f, 1.0f };
            desc.m_metallicFactor  = 0.0f;
            desc.m_roughnessFactor = 0.9f;
            desc.m_baseTexture     = &gpu->defaultTexture();
            desc.m_metRghTexture   = &gpu->defaultTexture();
            desc.m_emissionTexture = &gpu->defaultTexture();
            desc.m_normalTexture   = &gpu->defaultTexture();
            asset->m_materials.push_back( std::make_unique<TVkMaterial>( gpu->device(), gpu->physDevice(), gpu->descPool(), gpu->layouts().m_material, desc ) );
            app.assetSystem().registerAsset( std::move( asset ) );
            screenAsset = app.assetSystem().maybeGetAsset( "VideoScreen" );
        }
        if ( screenAsset == nullptr || screenAsset->m_meshes.empty() || screenAsset->m_materials.empty() ) return;

        TSceneNode& node = scene().createNode( "VideoScreen" );
        node.m_transform.setLocalTRS( { -2.5f, 1.5f, 0.0f }, glm::angleAxis( glm::radians( 90.0f ), glm::vec3( 1.0f, 0.0f, 0.0f ) ),
                                      glm::vec3( 1.2f, 1.0f, 0.8f ) );

        auto& mesh                 = node.emplaceComponent<TMeshComponent>( screenAsset->m_meshes[ 0 ].get(), screenAsset->m_materials[ 0 ].get(), false );
        mesh.m_baseTextureOverride = TBagAnimatedTextureRef{ "assets/textures/screen.mp4" };
        mesh.rebindOverrides( scene().resources(), *gpu );
        scene().addChild( &node );
    }

    void spawnWaterDemo()
    {
        auto& app = TApplication::get();
        auto* gpu = app.gpu();
        if ( gpu == nullptr ) return;

        TGpuAsset* waterAsset = app.assetSystem().maybeGetAsset( "WaterDemo" );
        if ( waterAsset == nullptr )
        {
            auto asset    = std::make_unique<TGpuAsset>();
            asset->m_name = "WaterDemo";
            asset->m_meshes.push_back( TMeshPrimitives::makePlane( *gpu ) );

            TVkMaterialDesc desc{};
            desc.m_baseColorFactor = { 0.05f, 0.28f, 0.38f, 1.0f };
            desc.m_metallicFactor  = 0.0f;
            desc.m_roughnessFactor = 0.08f;
            desc.m_normalScale     = 1.25f;
            desc.m_alphaMode       = TMatAlpha::Blend;
            desc.m_technique       = TMeshTechniqueId::Water;
            desc.m_doubleSided     = true;
            desc.m_baseTexture     = &gpu->defaultTexture();
            desc.m_metRghTexture   = &gpu->defaultTexture();
            desc.m_emissionTexture = &gpu->defaultTexture();
            desc.m_normalTexture   = &gpu->defaultTexture();
            asset->m_materials.push_back( std::make_unique<TVkMaterial>( gpu->device(), gpu->physDevice(), gpu->descPool(), gpu->layouts().m_material, desc ) );

            app.assetSystem().registerAsset( std::move( asset ) );
            waterAsset = app.assetSystem().maybeGetAsset( "WaterDemo" );
        }
        if ( waterAsset == nullptr || waterAsset->m_meshes.empty() || waterAsset->m_materials.empty() ) return;

        TSceneNode& node = scene().createNode( "WaterPlane" );
        node.m_transform.setTranslation( { 0.0f, 0.15f, -2.5f } );
        node.m_transform.setScale( { 8.0f, 1.0f, 8.0f } );
        node.emplaceComponent<TMeshComponent>( waterAsset->m_meshes[ 0 ].get(), waterAsset->m_materials[ 0 ].get(), false );
        scene().addChild( &node );

        TLOG_INFO() << "[Sandbox] Water demo: WaterPlane";
    }

    std::string m_modelPath;
};

class SandboxApp : public TApplication
{
public:
    // Explicit config path (games can also call TPath::setConfigPath before constructing).
    SandboxApp() : TApplication( TWindowProps{ "Tomos — Sandbox", 1280, 720 }, "tomos.json" )
    {
        // Validation is ~2/3 of CPU in Release profiles — keep it for Debug only.
        initGpu(
#if TOMOS_DEBUG
                true
#else
                false
#endif
        );

        // Register so the editor can restore scripts if a scene is loaded from UI.
        TScriptRegistry::get().registerType<FlyCameraScript>( "FlyCamera" );
        TScriptRegistry::get().registerType<AnimBlendDemoScript>( "AnimBlendDemo" );

        auto layer = std::make_unique<SandboxLayer>( "assets/sponza.glb" );
#ifdef TOMOS_EDITOR
        auto* layerPtr = layer.get();
#endif
        pushLayer( std::move( layer ) );

#ifdef TOMOS_EDITOR
        auto editor                   = std::make_unique<TSceneEditorLayer>( layerPtr->scene(), SandboxLayer::g_kScenePath );
        editor->context().m_afterLoad = [ layerPtr ] { layerPtr->bindGameplayHooks(); };
        pushOverlay( std::move( editor ) );
#endif
    }
};

int main()
{
    try
    {
        SandboxApp app;
        app.run();
    }
    catch ( const std::exception& e )
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
    return 0;
}
