cmake_minimum_required(VERSION 3.28)

# Guard against a second add_subdirectory of this engine root.
if(TARGET Tomos)
	return()
endif()

project(Tomos LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(TomosOptions)
include(TomosCompilerFlags)
include(TomosDependencies)
include(TomosDeploy)

add_library(Tomos STATIC
	src/Tomos/util/logger/TLogger.cc
	src/Tomos/util/logger/TLogger.hh
	src/Tomos/util/config/TConfig.hh
	src/Tomos/util/time/TTime.hh
	src/Tomos/util/time/TTime.cc
	src/Tomos/util/path/TPath.hh
	src/Tomos/util/path/TPath.cc
	src/Tomos/util/random/TRandom.hh
	src/Tomos/util/random/TRandom.cc
	# Memory (arenas / frame allocator / level store)
	src/Tomos/util/memory/TArena.hh
	src/Tomos/util/memory/TArena.cc
	src/Tomos/util/memory/TArenaAllocator.hh
	src/Tomos/util/memory/THandle.hh
	src/Tomos/util/memory/TPool.hh
	src/Tomos/util/memory/TFrameAllocator.hh
	src/Tomos/util/memory/TFrameAllocator.cc
	src/Tomos/util/memory/TLevelStore.hh
	src/Tomos/util/memory/TLevelStore.cc
	# Frame profiling (bodies compile out when TOMOS_DEBUG=0)
	src/Tomos/util/profile/TProfile.hh
	src/Tomos/util/profile/TFrameProfiler.hh
	src/Tomos/util/profile/TFrameProfiler.cc
	src/Tomos/util/profile/TGpuTimestamps.hh
	src/Tomos/util/profile/TGpuTimestamps.cc
	src/Tomos/util/profile/TFrameAnalytics.hh
	src/Tomos/util/profile/TFrameAnalytics.cc
	src/Tomos/core/events/TEvent.hh
	src/Tomos/core/events/TEvent.cc
	src/Tomos/core/events/mouse/TMouseEvent.cc
	src/Tomos/core/events/mouse/TMouseEvent.hh
	src/Tomos/core/events/key/TKeyEvent.cc
	src/Tomos/core/events/key/TKeyEvent.hh
	src/Tomos/core/events/application/TApplicationEvent.cc
	src/Tomos/core/events/application/TApplicationEvent.hh
	src/Tomos/core/window/TWindow.cc
	src/Tomos/core/window/TWindow.hh
	src/Tomos/core/input/TInput.hh
	src/Tomos/core/input/TInput.cc
	src/Tomos/core/input/TActionMap.hh
	src/Tomos/core/input/TActionMap.cc
	src/Tomos/core/app/TApplication.cc
	src/Tomos/core/app/TApplication.hh
	src/Tomos/core/layers/TLayer.hh
	src/Tomos/core/layers/TLayerStack.hh
	src/Tomos/core/layers/TLayerStack.cc
	src/Tomos/core/layers/TSceneLayer.hh
	src/Tomos/core/layers/TSceneLayer.cc
	src/Tomos/util/transform/TTransform.cc
	src/Tomos/util/transform/TTransform.hh
	src/Tomos/systems/TComponent.hh
	src/Tomos/systems/TSystem.hh
	# Script system
	src/Tomos/systems/script/TScript.hh
	src/Tomos/systems/script/TScriptComponent.hh
	src/Tomos/systems/script/TScriptUtils.hh
	src/Tomos/systems/script/TScriptRegistry.hh
	src/Tomos/systems/script/TScriptRegistry.cc
	src/Tomos/systems/script/TScriptSystem.hh
	src/Tomos/systems/script/TScriptSystem.cc
	# Camera system
	src/Tomos/systems/camera/TCameraComponent.hh
	src/Tomos/systems/camera/TCameraSystem.hh
	src/Tomos/systems/camera/TCameraSystem.cc
	# Light system
	src/Tomos/systems/light/TLightComponent.hh
	src/Tomos/systems/light/TLightSystem.hh
	src/Tomos/systems/light/TLightSystem.cc
	# Mesh system
	src/Tomos/systems/mesh/TMeshComponent.hh
	src/Tomos/systems/mesh/TMeshComponent.cc
	src/Tomos/systems/mesh/TMeshSystem.hh
	src/Tomos/systems/mesh/TMeshSystem.cc
	# Animation system
	src/Tomos/systems/animation/TAnimationClip.hh
	src/Tomos/systems/animation/TAnimStateMachine.hh
	src/Tomos/systems/animation/TAnimatorComponent.hh
	src/Tomos/systems/animation/TAnimatorComponent.cc
	src/Tomos/systems/animation/TAnimationSystem.hh
	src/Tomos/systems/animation/TAnimationSystem.cc
	# Sprite system
	src/Tomos/systems/sprite/TSpriteComponent.hh
	src/Tomos/systems/sprite/TSpriteSystem.hh
	src/Tomos/systems/sprite/TSpriteSystem.cc
	# Particle system
	src/Tomos/systems/particle/TParticleEmitterComponent.hh
	src/Tomos/systems/particle/TParticleSystem.hh
	src/Tomos/systems/particle/TParticleSystem.cc
	# Audio system
	src/Tomos/systems/audio/TAudioClip.hh
	src/Tomos/systems/audio/TAudioComponent.hh
	src/Tomos/systems/audio/TAudioSystem.hh
	src/Tomos/systems/audio/TAudioSystem.cc
	# Physics system
	src/Tomos/systems/physics/TRigidBodyComponent.hh
	src/Tomos/systems/physics/TColliderComponent.hh
	src/Tomos/systems/physics/TPhysicsLayers.hh
	src/Tomos/systems/physics/TPhysicsSystem.hh
	src/Tomos/systems/physics/TPhysicsSystem.cc
	# Asset system
	src/Tomos/systems/asset/TAssetHandles.hh
	src/Tomos/systems/asset/TAssetSystem.hh
	src/Tomos/systems/asset/TAssetLoadQueue.hh
	src/Tomos/systems/asset/TAssetLoadQueue.cc
	src/Tomos/core/ecs/TECS.hh
	src/Tomos/core/ecs/TECS.cc
	src/Tomos/core/scene/TSceneNode.hh
	src/Tomos/core/scene/TSceneNode.cc
	src/Tomos/core/scene/TScene.hh
	src/Tomos/core/scene/TScene.cc
	src/Tomos/core/scene/TSceneManager.hh
	src/Tomos/core/scene/TSceneManager.cc
	src/Tomos/core/scene/TSceneResourceBag.hh
	src/Tomos/core/scene/TSceneResourceBag.cc
	src/Tomos/core/scene/TSceneSerializer.hh
	src/Tomos/core/scene/TSceneSerializer.cc
	# Component registry (editor / serialization)
	src/Tomos/systems/TComponentRegistry.hh
	src/Tomos/systems/TComponentRegistry.cc
	# Mesh primitives + glTF loader
	src/Tomos/systems/mesh/TMeshPrimitives.hh
	src/Tomos/systems/mesh/TMeshPrimitives.cc
	src/Tomos/systems/asset/TGltfLoader.hh
	src/Tomos/systems/asset/TGltfLoader.cc
	src/Tomos/util/image/TImageLoad.hh
	src/Tomos/util/image/TImageLoad.cc
	src/Tomos/util/image/TAnimatedTextureDecoder.hh
	src/Tomos/util/image/TAnimatedTextureDecoder.cc
	src/Tomos/util/image/TAnimatedTexture.hh
	src/Tomos/util/image/TAnimatedTexture.cc
	# Animated texture system
	src/Tomos/systems/texture/TAnimatedTextureSystem.hh
	src/Tomos/systems/texture/TAnimatedTextureSystem.cc
	# GPU
	src/Tomos/gpu/TGpuEnums.hh
	src/Tomos/gpu/TRenderLimits.hh
	src/Tomos/gpu/TBillboardMode.hh
	# Vulkan backend
	src/Tomos/gpu/vulkan/TVkUtil.hh
	src/Tomos/gpu/vulkan/TVkBuffer.hh
	src/Tomos/gpu/vulkan/TVkBuffer.cc
	src/Tomos/gpu/vulkan/TVkImage.hh
	src/Tomos/gpu/vulkan/TVkImage.cc
	src/Tomos/gpu/vulkan/TVkMesh.hh
	src/Tomos/gpu/vulkan/TVkMaterial.hh
	src/Tomos/gpu/vulkan/TVkMaterial.cc
	src/Tomos/gpu/vulkan/TMeshTechnique.hh
	src/Tomos/gpu/vulkan/TMeshTechnique.cc
	src/Tomos/gpu/vulkan/TVkPass.hh
	src/Tomos/gpu/vulkan/TVkGpu.hh
	src/Tomos/gpu/vulkan/TVkGpu.cc
	src/Tomos/gpu/vulkan/TVkGpu.Swapchain.cc
	src/Tomos/gpu/vulkan/TVkGpu.Upload.cc
	# Clustered forward renderer
	src/Tomos/gpu/vulkan/renderer/TVkClusteredRenderer.hh
	src/Tomos/gpu/vulkan/renderer/TVkClusteredRenderer.cc
	src/Tomos/gpu/vulkan/renderer/TVkClusteredRenderer.Pipelines.cc
	src/Tomos/gpu/vulkan/renderer/TVkClusteredRenderer.Record.cc
	# Post-process stack
	src/Tomos/gpu/vulkan/post/TPostEffect.hh
	src/Tomos/gpu/vulkan/post/TPostUtil.hh
	src/Tomos/gpu/vulkan/post/TPostStack.hh
	src/Tomos/gpu/vulkan/post/TPostStack.cc
	src/Tomos/gpu/vulkan/post/TPostTonemap.hh
	src/Tomos/gpu/vulkan/post/TPostTonemap.cc
	src/Tomos/gpu/vulkan/post/TPostFog.hh
	src/Tomos/gpu/vulkan/post/TPostFog.cc
	src/Tomos/gpu/vulkan/post/TPostBloom.hh
	src/Tomos/gpu/vulkan/post/TPostBloom.cc
	src/Tomos/gpu/vulkan/post/TPostSAO.hh
	src/Tomos/gpu/vulkan/post/TPostSAO.cc
	src/Tomos/util/math/TFrustum.hh
	src/Tomos/util/math/TProjection.hh
	# UI
	src/Tomos/ui/TUiBackend.hh
	src/Tomos/ui/TUiLayer.hh
	src/Tomos/ui/TUiLayer.cc
	src/Tomos/ui/TImGuiBackend.hh
	src/Tomos/ui/TImGuiBackend.cc
	# Dear ImGui core + GLFW/Vulkan backends + ImGuizmo
	${imgui_SOURCE_DIR}/imgui.cpp
	${imgui_SOURCE_DIR}/imgui_draw.cpp
	${imgui_SOURCE_DIR}/imgui_tables.cpp
	${imgui_SOURCE_DIR}/imgui_widgets.cpp
	${imgui_SOURCE_DIR}/imgui_demo.cpp
	${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
	${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp
	${imguizmo_SOURCE_DIR}/src/ImGuizmo.cpp
)

set(TOMOS_SHADER_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/Tomos/gpu/vulkan/shaders")
set(TOMOS_RESOURCES_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/resources")

target_include_directories(Tomos
	PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src
	PUBLIC ${imgui_SOURCE_DIR}
	PUBLIC ${imgui_SOURCE_DIR}/backends
	PUBLIC ${imguizmo_SOURCE_DIR}/src
	PUBLIC ${STB_INCLUDE_DIR}
)

set_property(TARGET Tomos PROPERTY TOMOS_SHADER_INCLUDE_DIR "${TOMOS_SHADER_SRC_DIR}")
set_property(TARGET Tomos PROPERTY TOMOS_RESOURCES_DIR "${TOMOS_RESOURCES_SOURCE_DIR}")

target_compile_features(Tomos PUBLIC cxx_std_26)

target_compile_definitions(Tomos
	PUBLIC GLM_FORCE_DEPTH_ZERO_TO_ONE
	PUBLIC IMGUI_DEFINE_MATH_OPERATORS
	PUBLIC TOMOS_DEBUG=$<IF:$<CONFIG:Debug>,1,0>
	PUBLIC TOMOS_RESOURCES_DIR="${TOMOS_RESOURCES_SOURCE_DIR}"
)

tomos_enable_ipo(Tomos)

# PUBLIC: deps appear in installed/public headers (glm types, GLFW, Vulkan, json).
target_link_libraries(Tomos
	PUBLIC
		glm::glm
		glfw
		Vulkan::Vulkan
		nlohmann_json::nlohmann_json
		tomos_reflection_flags
	PRIVATE
		tomos_build_flags
		assimp::assimp
		miniaudio
		PkgConfig::FFMPEG
)

# Docked scene editor — separate library (not baked into Tomos).
set(TOMOS_EDITOR_SOURCES
	src/Tomos/ui/editor/TSceneEditorContext.hh
	src/Tomos/ui/editor/TDebugDraw.hh
	src/Tomos/ui/editor/TDebugDraw.cc
	src/Tomos/ui/editor/TAssetBrowserPanel.hh
	src/Tomos/ui/editor/TAssetBrowserPanel.cc
	src/Tomos/ui/editor/TSceneHierarchyPanel.hh
	src/Tomos/ui/editor/TSceneHierarchyPanel.cc
	src/Tomos/ui/editor/TSceneInspectorPanel.hh
	src/Tomos/ui/editor/TSceneInspectorPanel.cc
	src/Tomos/ui/editor/TSceneViewportPanel.hh
	src/Tomos/ui/editor/TSceneViewportPanel.cc
	src/Tomos/ui/editor/TRendererDebugPanel.hh
	src/Tomos/ui/editor/TRendererDebugPanel.cc
	src/Tomos/ui/editor/TPerformancePanel.hh
	src/Tomos/ui/editor/TPerformancePanel.cc
	src/Tomos/ui/editor/TPhysicsDebugPanel.hh
	src/Tomos/ui/editor/TPhysicsDebugPanel.cc
	src/Tomos/ui/editor/TConsolePanel.hh
	src/Tomos/ui/editor/TConsolePanel.cc
	src/Tomos/ui/editor/TSceneEditorLayer.hh
	src/Tomos/ui/editor/TSceneEditorLayer.cc
)
if(TOMOS_BUILD_EDITOR)
	add_library(TomosEditor STATIC ${TOMOS_EDITOR_SOURCES})
	target_link_libraries(TomosEditor PUBLIC Tomos)
	target_link_libraries(TomosEditor PRIVATE tomos_build_flags glm::glm nlohmann_json::nlohmann_json)
	target_compile_definitions(TomosEditor PUBLIC TOMOS_EDITOR=1)
	tomos_enable_ipo(TomosEditor)
	message(STATUS "TomosEditor: ON (TOMOS_BUILD_EDITOR)")
else()
	message(STATUS "TomosEditor: OFF (TOMOS_BUILD_EDITOR=OFF)")
endif()

# Engine GLSL → SPIR-V (same helper games use for custom shaders).
set(TOMOS_ENGINE_GLSL
	${TOMOS_SHADER_SRC_DIR}/forward.vert
	${TOMOS_SHADER_SRC_DIR}/forward.frag
	${TOMOS_SHADER_SRC_DIR}/water.vert
	${TOMOS_SHADER_SRC_DIR}/water.frag
	${TOMOS_SHADER_SRC_DIR}/shadow.vert
	${TOMOS_SHADER_SRC_DIR}/skinned.vert
	${TOMOS_SHADER_SRC_DIR}/skinned_shadow.vert
	${TOMOS_SHADER_SRC_DIR}/cluster_cull.comp
	${TOMOS_SHADER_SRC_DIR}/sprite.vert
	${TOMOS_SHADER_SRC_DIR}/sprite.frag
	${TOMOS_SHADER_SRC_DIR}/particle_sim.comp
	${TOMOS_SHADER_SRC_DIR}/particle.vert
	${TOMOS_SHADER_SRC_DIR}/particle.frag
	${TOMOS_SHADER_SRC_DIR}/fullscreen.vert
	${TOMOS_SHADER_SRC_DIR}/tonemap.frag
	${TOMOS_SHADER_SRC_DIR}/fog.frag
	${TOMOS_SHADER_SRC_DIR}/bloom_extract.frag
	${TOMOS_SHADER_SRC_DIR}/bloom_downsample.frag
	${TOMOS_SHADER_SRC_DIR}/bloom_upsample.frag
	${TOMOS_SHADER_SRC_DIR}/bloom_composite.frag
	${TOMOS_SHADER_SRC_DIR}/sao_linearize.frag
	${TOMOS_SHADER_SRC_DIR}/sao_sample.frag
	${TOMOS_SHADER_SRC_DIR}/sao_blur.frag
	${TOMOS_SHADER_SRC_DIR}/sao_temporal.frag
	${TOMOS_SHADER_SRC_DIR}/sao_compose.frag
)

tomos_compile_shaders(TomosShaders
	SOURCES ${TOMOS_ENGINE_GLSL}
	INCLUDE_DIRS ${TOMOS_SHADER_SRC_DIR}
	OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/shaders
)

get_property(_tomos_shader_out TARGET TomosShaders PROPERTY TOMOS_SHADER_OUTPUT_DIR)
# Dev fallback when the binary is not launched from its own directory.
target_compile_definitions(Tomos
	PUBLIC TOMOS_SHADER_DIR="${_tomos_shader_out}"
)

# Optional install of engine shaders and resources (packaging / distro layouts).
install(DIRECTORY ${_tomos_shader_out}/
	DESTINATION share/tomos/shaders
	OPTIONAL)
install(DIRECTORY ${TOMOS_RESOURCES_SOURCE_DIR}/
	DESTINATION share/tomos/resources
	OPTIONAL)
