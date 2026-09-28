# =============================================================================
# Example game CMakeLists.txt for Tomos
# =============================================================================
#
# Copy this file to the root of your game repo as CMakeLists.txt, then adjust
# names / paths. Assumed layout:
#
#   MyGame/
#     CMakeLists.txt          ← this file
#     src/main.cc
#     assets/                 ← meshes, audio, scenes, textures
#     shaders/                ← optional game-owned GLSL (list in tomos_compile_shaders)
#     external/Tomos/         ← git submodule of the TomosEngine repo
#
# Setup:
#   git submodule add <tomos-repo-url> external/Tomos
#   git submodule update --init --recursive
#
# Build:
#   cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
#   cmake --build build -j
#   ./build/MyGame
#
# After build, tomos_deploy_runtime places next to the binary:
#   assets/      — your ASSETS trees
#   shaders/     — engine SPIR-V (+ any SHADERS targets)
#   resources/   — engine Logo.png, etc.
#
# Game shaders are listed explicitly (same as the engine). Configure fails if a
# basename collides with an engine shader (e.g. forward.frag) — rename it
# (e.g. mygame_ripple.frag).
#
# =============================================================================

cmake_minimum_required(VERSION 3.28)
project(MyGame LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---------------------------------------------------------------------------
# Engine (submodule). Binary dir "_tomos" keeps FetchContent / shaders tidy.
# Optional: -DTOMOS_BUILD_EDITOR=OFF to skip the scene editor library.
# ---------------------------------------------------------------------------
add_subdirectory(external/Tomos ${CMAKE_BINARY_DIR}/_tomos)

add_executable(MyGame
	src/main.cc
)

# Tomos exports includes + C++26 / reflection flags publicly.
target_link_libraries(MyGame PRIVATE Tomos)

# Docked scene editor (built when TOMOS_BUILD_EDITOR=ON, the default).
if(TARGET TomosEditor)
	target_link_libraries(MyGame PRIVATE TomosEditor)
endif()

# Optional: same Release opts / LTO helper the engine uses.
target_link_libraries(MyGame PRIVATE tomos_build_flags)
tomos_enable_ipo(MyGame)

# ---------------------------------------------------------------------------
# Optional: compile game-owned GLSL → SPIR-V (list every file, like TomosShaders).
# Do NOT reuse engine basenames (forward.frag, tonemap.frag, …).
# Engine common/ headers are on the include path automatically.
# ---------------------------------------------------------------------------
# tomos_compile_shaders(MyGameShaders
# 	SOURCES
# 		${CMAKE_CURRENT_SOURCE_DIR}/shaders/mygame_effect.vert
# 		${CMAKE_CURRENT_SOURCE_DIR}/shaders/mygame_effect.frag
# )

# ---------------------------------------------------------------------------
# Stage runtime data next to MyGame (required for assets / shaders / logo).
# ---------------------------------------------------------------------------
tomos_deploy_runtime(MyGame
	ASSETS ${CMAKE_CURRENT_SOURCE_DIR}/assets
	# SHADERS MyGameShaders
	# RESOURCES ${CMAKE_CURRENT_SOURCE_DIR}/branding   # optional extras → resources/
)
