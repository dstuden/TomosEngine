cmake_minimum_required(VERSION 3.28)
project(Tomos)

set(CMAKE_CXX_STANDARD 23)

find_package(glm 1.0 CONFIG REQUIRED)
find_package(glfw3 3.4 REQUIRED)
find_package(Vulkan 1.3 REQUIRED)
find_package(nlohmann_json 3.12 REQUIRED)
find_package(assimp 6.0 REQUIRED)

# ---------------------------------------------------------------------------
# Dear ImGui (tooling / debug UI backend) — fetched, compiled into the engine.
# ---------------------------------------------------------------------------
include(FetchContent)
# CMP0169 OLD: allow FetchContent_Populate for header/source-only deps below
# (ImGuizmo, stb) so their upstream CMakeLists are not added as subprojects.
if(POLICY CMP0169)
	cmake_policy(SET CMP0169 OLD)
endif()
FetchContent_Declare(imgui
	GIT_REPOSITORY https://github.com/ocornut/imgui.git
	GIT_TAG        v1.91.9b-docking
	GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(imgui)

# ImGuizmo — transform manipulators (sources only; we compile ImGuizmo.cpp into Tomos).
# Pinned to a commit compatible with imgui v1.91.9b-docking (ImGuizmo v1.92.5 WIP).
# Populate (not MakeAvailable) — compile ImGuizmo.cpp ourselves.
FetchContent_Declare(imguizmo
	GIT_REPOSITORY https://github.com/CedricGuillemet/ImGuizmo.git
	GIT_TAG        18cef5e031d8c6973d80284c67f60549fafd78c1
)
FetchContent_GetProperties(imguizmo)
if(NOT imguizmo_POPULATED)
	FetchContent_Populate(imguizmo)
endif()

# ---------------------------------------------------------------------------
# stb (stb_image) — header-only; expose as <stb/stb_image.h> like distro packages.
# Populate (not MakeAvailable) — header-only, no upstream subproject.
# ---------------------------------------------------------------------------
FetchContent_Declare(stb
	GIT_REPOSITORY https://github.com/nothings/stb.git
	GIT_TAG        2c980bb59875b0d32144a71867fbdebb2f77cd20
)
FetchContent_GetProperties(stb)
if(NOT stb_POPULATED)
	FetchContent_Populate(stb)
endif()
if(NOT EXISTS "${stb_SOURCE_DIR}/stb_image.h")
	message(FATAL_ERROR
		"stb_image.h missing after FetchContent (expected at ${stb_SOURCE_DIR}/stb_image.h).")
endif()
set(STB_INCLUDE_DIR "${CMAKE_CURRENT_BINARY_DIR}/_stb_include")
file(MAKE_DIRECTORY "${STB_INCLUDE_DIR}/stb")
configure_file(
	"${stb_SOURCE_DIR}/stb_image.h"
	"${STB_INCLUDE_DIR}/stb/stb_image.h"
	COPYONLY
)

# ---------------------------------------------------------------------------
# miniaudio (audio playback) — fetched; we link the static lib (no local IMPLEMENTATION).
# ---------------------------------------------------------------------------
set(MINIAUDIO_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(MINIAUDIO_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(MINIAUDIO_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
set(MINIAUDIO_NO_LIBVORBIS ON CACHE BOOL "" FORCE)
set(MINIAUDIO_NO_LIBOPUS ON CACHE BOOL "" FORCE)
set(MINIAUDIO_NO_EXTRA_NODES ON CACHE BOOL "" FORCE)
set(MINIAUDIO_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_Declare(miniaudio
	GIT_REPOSITORY https://github.com/mackron/miniaudio.git
	GIT_TAG        0.11.25
	GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(miniaudio)

find_package(PkgConfig REQUIRED)
pkg_check_modules(FFMPEG REQUIRED IMPORTED_TARGET
	libavformat libavcodec libavutil libswscale)

add_library(${PROJECT_NAME} STATIC
	src/Tomos/util/logger/TLogger.cc
	src/Tomos/util/logger/TLogger.hh
	src/Tomos/util/config/TConfig.hh
	src/Tomos/util/time/TTime.hh
	src/Tomos/util/time/TTime.cc
	src/Tomos/util/path/TPath.hh
	src/Tomos/util/path/TPath.cc
	src/Tomos/util/random/TRandom.hh
	src/Tomos/util/random/TRandom.cc
	src/Tomos/util/shader/TShaderHotReload.hh
	src/Tomos/util/shader/TShaderHotReload.cc
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

# Automatically include all directories with headers
target_include_directories(${PROJECT_NAME}
	PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src
	PUBLIC ${imgui_SOURCE_DIR}
	PUBLIC ${imgui_SOURCE_DIR}/backends
	PUBLIC ${imguizmo_SOURCE_DIR}/src
	PUBLIC ${STB_INCLUDE_DIR}
)

target_compile_definitions(${PROJECT_NAME}
	PUBLIC GLM_FORCE_DEPTH_ZERO_TO_ONE
	PUBLIC IMGUI_DEFINE_MATH_OPERATORS
	PUBLIC $<$<CONFIG:Debug>:TOMOS_DEBUG>
	PUBLIC $<$<CONFIG:Release>:NDEBUG>
	PUBLIC TOMOS_RESOURCES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/resources"
)

# Release: interprocedural optimization (LTO) when the toolchain supports it.
include(CheckIPOSupported)
check_ipo_supported(RESULT TOMOS_IPO_SUPPORTED OUTPUT TOMOS_IPO_ERROR)
if(TOMOS_IPO_SUPPORTED)
	set_property(TARGET ${PROJECT_NAME} PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
else()
	message(STATUS "IPO/LTO not available for Release: ${TOMOS_IPO_ERROR}")
endif()

# Extra CPU opts for local Release / RelWithDebInfo builds (GCC/Clang; skip MSVC).
# --gc-sections is GNU ld / ELF-oriented; skip on MSVC and Apple ld.
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
	target_compile_options(${PROJECT_NAME} PRIVATE
		$<$<OR:$<CONFIG:Release>,$<CONFIG:RelWithDebInfo>>:-O3 -ffunction-sections -fdata-sections>
	)
	if(NOT APPLE)
		target_link_options(${PROJECT_NAME} PRIVATE
			$<$<OR:$<CONFIG:Release>,$<CONFIG:RelWithDebInfo>>:-Wl,--gc-sections>
		)
	endif()
endif()

target_link_libraries(${PROJECT_NAME}
	PRIVATE
		glm::glm
		glfw
		Vulkan::Vulkan
		nlohmann_json::nlohmann_json
		assimp::assimp
		miniaudio
		PkgConfig::FFMPEG
)

# Docked scene editor — separate library (not baked into Tomos).
# Sources live under Tomos/ui/editor but belong to the TomosEditor target.
option(TOMOS_BUILD_EDITOR "Build TomosEditor library (docked scene editor)" ON)
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
	src/Tomos/ui/editor/TPhysicsDebugPanel.hh
	src/Tomos/ui/editor/TPhysicsDebugPanel.cc
	src/Tomos/ui/editor/TConsolePanel.hh
	src/Tomos/ui/editor/TConsolePanel.cc
	src/Tomos/ui/editor/TSceneEditorLayer.hh
	src/Tomos/ui/editor/TSceneEditorLayer.cc
)
if(TOMOS_BUILD_EDITOR)
	add_library(TomosEditor STATIC ${TOMOS_EDITOR_SOURCES})
	target_link_libraries(TomosEditor PUBLIC ${PROJECT_NAME})
	target_link_libraries(TomosEditor PRIVATE glm::glm nlohmann_json::nlohmann_json)
	target_compile_definitions(TomosEditor PUBLIC TOMOS_EDITOR=1)
	target_compile_definitions(TomosEditor PRIVATE
		GLM_FORCE_DEPTH_ZERO_TO_ONE
		IMGUI_DEFINE_MATH_OPERATORS
		$<$<CONFIG:Debug>:TOMOS_DEBUG>
		$<$<CONFIG:Release>:NDEBUG>
	)
	if(TOMOS_IPO_SUPPORTED)
		set_property(TARGET TomosEditor PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
	endif()
	if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
		target_compile_options(TomosEditor PRIVATE
			$<$<OR:$<CONFIG:Release>,$<CONFIG:RelWithDebInfo>>:-O3 -ffunction-sections -fdata-sections>
		)
	endif()
	message(STATUS "TomosEditor: ON (TOMOS_BUILD_EDITOR)")
else()
	message(STATUS "TomosEditor: OFF (TOMOS_BUILD_EDITOR=OFF)")
endif()

# ---------------------------------------------------------------------------
# GLSL → SPIR-V compilation (requires glslc from the Vulkan SDK / shaderc)
# ---------------------------------------------------------------------------
find_program(GLSLC glslc HINTS ${Vulkan_GLSLC_EXECUTABLE})
if(NOT GLSLC)
	message(FATAL_ERROR
		"glslc not found. Install the Vulkan SDK or a shaderc package and ensure glslc is on PATH.\n"
		"  Arch:     pacman -S shaderc\n"
		"  Debian:   apt install glslc\n"
		"  macOS:    brew install shaderc  (or LunarG Vulkan SDK)\n"
		"  Windows:  install LunarG Vulkan SDK and add its Bin directory to PATH")
endif()

set(SHADER_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src/Tomos/gpu/vulkan/shaders)
set(SHADER_OUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/shaders)
file(MAKE_DIRECTORY ${SHADER_OUT_DIR})

set(GLSL_SOURCES
	${SHADER_SRC_DIR}/forward.vert
	${SHADER_SRC_DIR}/forward.frag
	${SHADER_SRC_DIR}/water.vert
	${SHADER_SRC_DIR}/water.frag
	${SHADER_SRC_DIR}/shadow.vert
	${SHADER_SRC_DIR}/skinned.vert
	${SHADER_SRC_DIR}/skinned_shadow.vert
	${SHADER_SRC_DIR}/cluster_cull.comp
	${SHADER_SRC_DIR}/sprite.vert
	${SHADER_SRC_DIR}/sprite.frag
	${SHADER_SRC_DIR}/particle_sim.comp
	${SHADER_SRC_DIR}/particle.vert
	${SHADER_SRC_DIR}/particle.frag
	${SHADER_SRC_DIR}/fullscreen.vert
	${SHADER_SRC_DIR}/tonemap.frag
	${SHADER_SRC_DIR}/fog.frag
	${SHADER_SRC_DIR}/bloom_extract.frag
	${SHADER_SRC_DIR}/bloom_blur.frag
	${SHADER_SRC_DIR}/bloom_composite.frag
	${SHADER_SRC_DIR}/sao_linearize.frag
	${SHADER_SRC_DIR}/sao_sample.frag
	${SHADER_SRC_DIR}/sao_blur.frag
	${SHADER_SRC_DIR}/sao_temporal.frag
	${SHADER_SRC_DIR}/sao_compose.frag
)

# Shared GLSL headers (#include "common/…"); -I so paths are relative to shaders/.
file(GLOB SHADER_COMMON_HEADERS CONFIGURE_DEPENDS "${SHADER_SRC_DIR}/common/*.glsl")

# Optimize SPIR-V in Release-like builds (glslc -O); keep debug shaders unoptimized.
set(GLSLC_FLAGS "")
if(CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
	list(APPEND GLSLC_FLAGS -O)
endif()
list(APPEND GLSLC_FLAGS -I${SHADER_SRC_DIR})

set(SPV_OUTPUTS "")
foreach(GLSL IN LISTS GLSL_SOURCES)
	get_filename_component(FILE_NAME ${GLSL} NAME)
	set(SPV ${SHADER_OUT_DIR}/${FILE_NAME}.spv)
	add_custom_command(
		OUTPUT ${SPV}
		COMMAND ${GLSLC} ${GLSLC_FLAGS} -o ${SPV} ${GLSL}
		DEPENDS ${GLSL} ${SHADER_COMMON_HEADERS}
		COMMENT "Compiling ${FILE_NAME} → ${FILE_NAME}.spv"
	)
	list(APPEND SPV_OUTPUTS ${SPV})
endforeach()

add_custom_target(TomosShaders ALL DEPENDS ${SPV_OUTPUTS})

# Fallback path when the binary is not launched from its own directory.
# Shader source + glslc paths enable Debug hot-reload (F5 / file watch).
target_compile_definitions(${PROJECT_NAME}
	PUBLIC TOMOS_SHADER_DIR="${SHADER_OUT_DIR}"
	PUBLIC TOMOS_SHADER_SRC_DIR="${SHADER_SRC_DIR}"
	PUBLIC TOMOS_GLSLC="${GLSLC}"
)
