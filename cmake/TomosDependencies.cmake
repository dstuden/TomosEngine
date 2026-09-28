# Host packages + FetchContent deps for the Tomos static library.
# Included from the engine root CMakeLists.txt only.

# Host-only: Vulkan SDK / loader + FFmpeg (heavy native stacks; not FetchContent).
find_package(Vulkan 1.3 REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(FFMPEG REQUIRED IMPORTED_TARGET
	libavformat libavcodec libavutil libswscale)

include(FetchContent)
# CMP0169 OLD: allow FetchContent_Populate for header/source-only deps below
# (ImGuizmo, stb) so their upstream CMakeLists are not added as subprojects.
if(POLICY CMP0169)
	cmake_policy(SET CMP0169 OLD)
endif()

set(FETCHCONTENT_TRY_FIND_PACKAGE_MODE NEVER)

# Prefer static libs from fetched deps (links cleanly into libTomos.a).
# Scope the FORCE to this include so a parent game can still build shared libs.
if(DEFINED BUILD_SHARED_LIBS)
	set(_TOMOS_SAVED_BUILD_SHARED_LIBS "${BUILD_SHARED_LIBS}")
	set(_TOMOS_HAD_BUILD_SHARED_LIBS TRUE)
else()
	set(_TOMOS_HAD_BUILD_SHARED_LIBS FALSE)
endif()
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

# glm ≥ 1.0
set(GLM_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLM_BUILD_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_Declare(glm
	GIT_REPOSITORY https://github.com/g-truc/glm.git
	GIT_TAG        1.0.3
	GIT_SHALLOW    TRUE
	OVERRIDE_FIND_PACKAGE
)
FetchContent_MakeAvailable(glm)

# glfw ≥ 3.4 — on Linux build both Wayland and X11; GLFW picks the backend at
# runtime (WAYLAND_DISPLAY / DISPLAY). Needs wayland + wayland-protocols +
# libxkbcommon (and the usual X11 client libs) on the host to compile.
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
if(UNIX AND NOT APPLE)
	set(GLFW_BUILD_WAYLAND ON CACHE BOOL "" FORCE)
	set(GLFW_BUILD_X11 ON CACHE BOOL "" FORCE)
endif()
FetchContent_Declare(glfw
	GIT_REPOSITORY https://github.com/glfw/glfw.git
	GIT_TAG        3.5.1
	GIT_SHALLOW    TRUE
	OVERRIDE_FIND_PACKAGE
)
FetchContent_MakeAvailable(glfw)

# nlohmann_json ≥ 3.12
set(JSON_BuildTests OFF CACHE BOOL "" FORCE)
set(JSON_Install OFF CACHE BOOL "" FORCE)
FetchContent_Declare(nlohmann_json
	GIT_REPOSITORY https://github.com/nlohmann/json.git
	GIT_TAG        v3.12.0
	GIT_SHALLOW    TRUE
	OVERRIDE_FIND_PACKAGE
)
FetchContent_MakeAvailable(nlohmann_json)

# assimp ≥ 6 — import-only (glTF via TGltfLoader); vendored zlib.
# Only enable the GLTF importer to cut compile time / binary size.
set(ASSIMP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_SAMPLES OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_ASSIMP_TOOLS OFF CACHE BOOL "" FORCE)
set(ASSIMP_INSTALL OFF CACHE BOOL "" FORCE)
set(ASSIMP_WARNINGS_AS_ERRORS OFF CACHE BOOL "" FORCE)
set(ASSIMP_INJECT_DEBUG_POSTFIX OFF CACHE BOOL "" FORCE)
set(ASSIMP_IGNORE_GIT_HASH ON CACHE BOOL "" FORCE)
set(ASSIMP_NO_EXPORT ON CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_ZLIB ON CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_ALL_IMPORTERS_BY_DEFAULT OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_GLTF_IMPORTER ON CACHE BOOL "" FORCE)
FetchContent_Declare(assimp
	GIT_REPOSITORY https://github.com/assimp/assimp.git
	GIT_TAG        v6.0.5
	GIT_SHALLOW    TRUE
	OVERRIDE_FIND_PACKAGE
)
FetchContent_MakeAvailable(assimp)

# ---------------------------------------------------------------------------
# Dear ImGui (tooling / debug UI backend) — fetched, compiled into the engine.
# ---------------------------------------------------------------------------
FetchContent_Declare(imgui
	GIT_REPOSITORY https://github.com/ocornut/imgui.git
	GIT_TAG        v1.91.9b-docking
	GIT_SHALLOW    TRUE
	OVERRIDE_FIND_PACKAGE
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
	OVERRIDE_FIND_PACKAGE
)
FetchContent_MakeAvailable(miniaudio)

# Restore parent BUILD_SHARED_LIBS (do not leave FORCE on for games).
if(_TOMOS_HAD_BUILD_SHARED_LIBS)
	set(BUILD_SHARED_LIBS ${_TOMOS_SAVED_BUILD_SHARED_LIBS} CACHE BOOL "" FORCE)
else()
	unset(BUILD_SHARED_LIBS CACHE)
endif()
unset(_TOMOS_SAVED_BUILD_SHARED_LIBS)
unset(_TOMOS_HAD_BUILD_SHARED_LIBS)
