# The Tomos Game Engine

The Tomos Game Engine is a 2D/3D game engine written in C++ and Vulkan. It is designed to be simple and easy to use,
while still being powerful and flexible. The engine is still in development.

**Developing with Tomos:** see [`docs/DEVELOPING.md`](docs/DEVELOPING.md) for architecture,
frame flow, and recipes (scenes, materials, shaders, lights, sprites, UI, post effects,
frustum culling).

Current rendering path: **clustered forward** (shadows → light cull → GGX) into HDR,
then **world sprites** and **GPU additive particles**, then a **post stack**
(SAO / fog / bloom / tonemap), plus GPU-skinned meshes and ImGui overlays.

---

Right now the engine is in a very early stage of development. The engine is being developed on Linux, but it should be possible to build it on Windows and
MacOS as well.

## Shipped vs planned

**Already in tree:** scene graph + ECS, scripts (`TScript`), input events / action maps,
clustered forward rendering, shadows (dir/spot/point cubemap), GPU skinning, world sprites, GPU additive
particles, ImGui overlays, scene editor (hierarchy / inspector / JSON / asset browser), post stack
(SAO / fog / bloom / tonemap), spatial audio (`TAudioComponent`), fixed-timestep physics
(`TPhysicsSystem` + rigid body / collider / triggers), engine config (`tomos.json` via
`TConfigManager`), scene switching (`TSceneManager`), texture mipmaps,
animated textures (GIF / WebP / video via FFmpeg).

**Still planned:** networking, HTML UI backend, broader cross-platform polish.

## Dependencies

Refer to `CMakeLists.txt` / `cmake/TomosDependencies.cmake` for the most up-to-date list. Host packages:

| Component | Notes |
|-----------|--------|
| CMake ≥ 3.28, C++26 toolchain (GCC 16+ for reflection) | |
| Vulkan 1.3 + **glslc** | LunarG Vulkan SDK, or distro `vulkan` / `shaderc` (`glslc` must be on `PATH`) |
| **FFmpeg** (libavformat / libavcodec / libavutil / libswscale) | via **pkg-config** (`PkgConfig::FFMPEG`); animated textures |
| X11 + Wayland client libs (Linux) | needed to *build* GLFW (both backends; runtime picks one) |
| glm, glfw, assimp, nlohmann_json, Dear ImGui, ImGuizmo, miniaudio, stb | fetched via CMake `FetchContent` (pinned) |

Pinned FetchContent tags (see `cmake/TomosDependencies.cmake`): glm `1.0.3`, glfw `3.5.1`,
nlohmann_json `v3.12.0`, assimp `v6.0.5`, Dear ImGui `v1.91.9b-docking`, miniaudio `0.11.25`,
ImGuizmo and stb pinned by commit hash. The first configure needs network access to
fetch them; later configures reuse `_deps` under the **engine binary dir** (e.g.
`build/_deps` when building the engine alone, or `build/_tomos/_deps` when a game
uses `add_subdirectory(... ${CMAKE_BINARY_DIR}/_tomos)`).

### Host packages (one-liners)

```bash
# Arch
sudo pacman -S --needed cmake ninja gcc vulkan-devel shaderc ffmpeg \
  libx11 libxrandr libxinerama libxcursor libxi \
  wayland wayland-protocols libxkbcommon pkgconf

# Debian / Ubuntu
sudo apt install build-essential cmake ninja-build pkg-config libvulkan-dev glslc \
  libavformat-dev libavcodec-dev libavutil-dev libswscale-dev \
  libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev \
  libwayland-dev wayland-protocols libxkbcommon-dev

# macOS (Homebrew) — also install the LunarG Vulkan SDK for validation layers / MoltenVK as needed
brew install cmake ninja pkg-config shaderc ffmpeg
```

`glslc` comes from **shaderc** or the **Vulkan SDK**. Configure fails with a clear error if it is missing.
FFmpeg is resolved with **pkg-config** (`find_package(PkgConfig)` + `pkg_check_modules`).

## Using Tomos in your game

Add this repo as a **git submodule** (or subtree) and `add_subdirectory` it:

```
MyGame/
  CMakeLists.txt
  src/...
  assets/
  external/Tomos/          # this repository
```

```bash
git submodule add <tomos-repo-url> external/Tomos
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
./build/MyGame
```

**Copy-paste CMake:** [`docs/examples/MyGame.CMakeLists.txt`](docs/examples/MyGame.CMakeLists.txt)
(annotated template — rename the project/target and point `add_subdirectory` at your
submodule path).

Minimal shape:

```cmake
add_subdirectory(external/Tomos)
add_executable(MyGame src/main.cc)
target_link_libraries(MyGame PRIVATE Tomos)   # + TomosEditor if wanted
# Optional custom GLSL (list files explicitly, like the engine):
# tomos_compile_shaders(MyGameShaders SOURCES shaders/mygame_effect.frag …)
tomos_deploy_runtime(MyGame
  ASSETS ${CMAKE_CURRENT_SOURCE_DIR}/assets
  # SHADERS MyGameShaders
  # RESOURCES ${CMAKE_CURRENT_SOURCE_DIR}/branding   # optional extras → resources/
)
```

`tomos_deploy_runtime` stages next to the executable:

| Dir | Contents |
|-----|----------|
| `assets/` | your game assets (`ASSETS`) |
| `shaders/` | engine SPIR-V (+ game shaders from `SHADERS`) |
| `resources/` | engine branding (`Logo.png`, …) plus any `RESOURCES` dirs |

Game shaders are listed manually via `tomos_compile_shaders(... SOURCES …)` (same as
`TomosShaders`). Configure **fails** if a basename collides with an engine shader — prefer
names like `mygame_ripple.frag`, not `forward.frag`.

`TPath` discovers the asset root from the binary directory / `tomos.json`. Shaders and the window icon also resolve under that layout (with compile-time `TOMOS_*_DIR` fallbacks for IDE runs).

Helpers live in [`cmake/TomosDeploy.cmake`](cmake/TomosDeploy.cmake) and are available after `add_subdirectory`. Game code patterns (`TApplication` / `TSceneLayer`) are in [`docs/DEVELOPING.md`](docs/DEVELOPING.md).

## Building the engine alone

Useful when iterating on the library without a game. This builds **libraries only**
(`Tomos`, optional `TomosEditor`, and `TomosShaders`) — there is no sample executable
in-tree.

Presets use the **Unix Makefiles** generator (`build` / `build-release`):

```bash
cmake --preset debug
cmake --build --preset debug -j
```

Release: `cmake --preset release` / `cmake --build --preset release -j`.

Equivalent without presets:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build -j
```

Optional **Ninja**: `cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug` (or pass
`-G Ninja` when configuring a game). Skip the editor library with
`-DTOMOS_BUILD_EDITOR=OFF`.
