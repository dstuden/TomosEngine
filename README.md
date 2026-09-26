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

Refer to `TomosEngine/CMakeLists.txt` for the most up-to-date list. Host packages:

| Component | Notes |
|-----------|--------|
| CMake ≥ 3.28, C++23 toolchain | |
| Vulkan 1.3 + **glslc** | LunarG Vulkan SDK, or distro `vulkan` / `shaderc` (`glslc` must be on `PATH`) |
| **FFmpeg** (libavformat / libavcodec / libavutil / libswscale) | animated textures (GIF, WebP, video) |
| X11 + Wayland client libs (Linux) | needed to *build* GLFW (both backends; runtime picks one) |
| glm, glfw, assimp, nlohmann_json, Dear ImGui, ImGuizmo, miniaudio, stb | fetched via CMake `FetchContent` (pinned) |

Pinned FetchContent tags (see `TomosEngine/CMakeLists.txt`): glm `1.0.3`, glfw `3.5.1`,
nlohmann_json `v3.12.0`, assimp `v6.0.5`, Dear ImGui `v1.91.9b-docking`, miniaudio `0.11.25`,
ImGuizmo and stb pinned by commit hash. The first configure needs network access to
fetch them; later configures reuse `build*/_deps`.

### Host packages (one-liners)

```bash
# Arch
sudo pacman -S --needed cmake ninja gcc vulkan-devel shaderc ffmpeg \
  libx11 libxrandr libxinerama libxcursor libxi \
  wayland wayland-protocols libxkbcommon

# Debian / Ubuntu
sudo apt install build-essential cmake ninja-build libvulkan-dev glslc \
  libavformat-dev libavcodec-dev libavutil-dev libswscale-dev \
  libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev \
  libwayland-dev wayland-protocols libxkbcommon-dev

# macOS (Homebrew) — also install the LunarG Vulkan SDK for validation layers / MoltenVK as needed
brew install cmake ninja shaderc ffmpeg
```

`glslc` comes from **shaderc** or the **Vulkan SDK**. Configure fails with a clear error if it is missing.

## Building

From the **repo root** (this tree contains both `TomosEngine/` and `Sandbox/`):

```bash
cmake --preset debug
cmake --build --preset debug -j
cd build/Sandbox && ./SandBox
```

Release:

```bash
cmake --preset release
cmake --build --preset release -j
cd build-release/Sandbox && ./SandBox
```

Equivalent without presets:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build -j
```

Run from the Sandbox binary directory so `assets/` and `shaders/` resolve
(`TPath` also discovers the asset root from the binary dir / `tomos.json`).
CLion: use the **Release** / **Debug** CMake profiles (or **SandBox (Release)**).

Optional install layout (after a build): `cmake --install build --prefix /path/to/prefix`
(places `SandBox` under `bin/` and shaders/assets/resources under `share/tomos/`).
