cmake_minimum_required(VERSION 3.28)
project(Tomos)

set(CMAKE_CXX_STANDARD 23)

find_package(glm 1.0 CONFIG REQUIRED)
find_package(glfw3 3.4 REQUIRED)
find_package(GLEW 2.1 REQUIRED)
find_package(OpenGL REQUIRED)
find_package(Jolt REQUIRED)
find_package(assimp 6 REQUIRED)

# Gather all .cc files recursively
file(GLOB_RECURSE ENGINE_SOURCES CONFIGURE_DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cc
)

# Gather all header directories for include paths
file(GLOB_RECURSE ENGINE_HEADERS CONFIGURE_DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/src/*.hh
)

add_library(${PROJECT_NAME} STATIC
        src/Tomos/util/logger/Logger.cc
        src/Tomos/util/logger/Logger.hh
        src/Tomos/core/Window.cc
        src/Tomos/core/Window.hh
        src/Tomos/events/Event.cc
        src/Tomos/events/Event.hh
        src/Tomos/events/key/KeyEvent.cc
        src/Tomos/events/key/KeyEvent.hh
        src/Tomos/events/mouse/MouseEvent.cc
        src/Tomos/events/mouse/MouseEvent.hh
        src/Tomos/events/application/ApplicationEvent.cc
        src/Tomos/events/application/ApplicationEvent.hh
        src/Tomos/core/Application.cc
        src/Tomos/core/Application.hh
        src/Tomos/util/renderer/Shader.cc
        src/Tomos/util/renderer/Shader.hh
        src/Tomos/core/Node.cc
        src/Tomos/core/Node.hh
        src/Tomos/lib/stb_image.h
        src/Tomos/core/Ecs.cc
        src/Tomos/core/Ecs.hh
        src/Tomos/systems/System.hh
        src/Tomos/systems/Component.hh
        src/Tomos/util/transform/Transform.cc
        src/Tomos/util/transform/Transform.hh
        src/Tomos/systems/camera/CameraComponent.cc
        src/Tomos/systems/camera/CameraComponent.hh
        src/Tomos/systems/camera/CameraSystem.cc
        src/Tomos/systems/camera/CameraSystem.hh
        src/Tomos/util/input/Input.cc
        src/Tomos/util/input/Input.hh
        src/Tomos/core/Layer.cc
        src/Tomos/core/Layer.hh
        src/Tomos/util/renderer/Buffer.cc
        src/Tomos/util/renderer/Buffer.hh
        src/Tomos/util/renderer/VertexArray.cc
        src/Tomos/util/renderer/VertexArray.hh
        src/Tomos/util/renderer/Renderer.cc
        src/Tomos/util/renderer/Renderer.hh
        src/Tomos/util/time/Time.cc
        src/Tomos/util/time/Time.hh
        src/Tomos/util/resourceManager/ResourceManager.hh
        src/Tomos/systems/script/ScriptSystem.cc
        src/Tomos/systems/script/ScriptSystem.hh
        src/Tomos/systems/script/ScriptComponent.hh
        src/Tomos/lib/lib.cc
        src/Tomos/lib/json.hpp
        src/Tomos/systems/mesh/MeshComponent.hh
        src/Tomos/systems/mesh/MeshSystem.cc
        src/Tomos/systems/mesh/MeshSystem.hh
        src/Tomos/systems/mesh/Mesh.cc
        src/Tomos/systems/mesh/Mesh.hh
        src/Tomos/util/gltfLoader/GLBLoader.cc
        src/Tomos/util/gltfLoader/GLBLoader.hh
        src/Tomos/util/renderer/Texture.cc
        src/Tomos/util/renderer/Texture.hh
        src/Tomos/util/renderer/Sampler.cc
        src/Tomos/util/renderer/Sampler.hh
        src/Tomos/systems/physics/PhysicsSystem.cc
        src/Tomos/systems/physics/PhysicsSystem.hh
        src/Tomos/util/imgui/ImGuiLayer.cc
        src/Tomos/util/imgui/ImGuiLayer.hh
        src/Tomos/lib/imgui/imgui.cpp
        src/Tomos/lib/imgui/imgui_demo.cpp
        src/Tomos/lib/imgui/imgui_draw.cpp
        src/Tomos/lib/imgui/imgui_impl_glfw.cpp
        src/Tomos/lib/imgui/imgui_impl_opengl3.cpp
        src/Tomos/lib/imgui/imgui_impl_opengl3_loader.h
        src/Tomos/lib/imgui/imgui_internal.h
        src/Tomos/lib/imgui/imgui_tables.cpp
        src/Tomos/lib/imgui/imgui_widgets.cpp
        src/Tomos/lib/imgui/imstb_rectpack.h
        src/Tomos/lib/imgui/imstb_textedit.h
        src/Tomos/lib/imgui/imstb_truetype.h
        src/Tomos/util/renderer/Material.cc
        src/Tomos/util/renderer/Material.hh
        src/Tomos/util/renderer/FrameBuffer.cc
        src/Tomos/util/renderer/FrameBuffer.hh
        src/Tomos/core/Scene.cc
        src/Tomos/core/Scene.hh
        src/Tomos/util/conf/Config.hh
        src/Tomos/util/renderer/passes/RenderPass.hh
        src/Tomos/util/resourceManager/ResourceManager.cc
        src/Tomos/util/renderer/passes/mesh/GBufferPass.cc
        src/Tomos/util/renderer/passes/mesh/GBufferPass.hh
        src/Tomos/systems/light/LightComponent.hh
        src/Tomos/systems/light/LightSystem.cc
        src/Tomos/systems/light/LightSystem.hh
        src/Tomos/util/renderer/passes/light/LightPass.cc
        src/Tomos/util/renderer/passes/light/LightPass.hh
)

# Set paths for engine resources
set(ENGINE_RESOURCE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/Tomos/resources")

# Copy engine resources to game binary directory (will be set up when game is built)
if (EXISTS "${ENGINE_RESOURCE_SOURCE_DIR}")
    file(GLOB_RECURSE ENGINE_RESOURCE_FILES
            LIST_DIRECTORIES false
            RELATIVE "${ENGINE_RESOURCE_SOURCE_DIR}"
            CONFIGURE_DEPENDS
            "${ENGINE_RESOURCE_SOURCE_DIR}/*")

    message(STATUS "Found ${ENGINE_RESOURCE_FILES} engine resource files to copy")

    # Create a custom target for engine resource copying
    add_custom_target(CopyEngineResources ALL
            COMMENT "Engine resources ready for copying..."
    )

    # Store engine resources info for the game to use
    set_target_properties(CopyEngineResources PROPERTIES
            ENGINE_RESOURCE_SOURCE_DIR "${ENGINE_RESOURCE_SOURCE_DIR}"
            ENGINE_RESOURCE_FILES "${ENGINE_RESOURCE_FILES}"
    )
else ()
    message(STATUS "Engine resource directory not found: ${ENGINE_RESOURCE_SOURCE_DIR}")
endif ()

# Automatically include all directories with headers
target_include_directories(${PROJECT_NAME} PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

if (CMAKE_BUILD_TYPE STREQUAL "Release")
    add_definitions(-DNDEBUG)
endif ()

target_link_libraries(${PROJECT_NAME} PRIVATE glm::glm glfw GLEW::GLEW OpenGL::GL Jolt::Jolt assimp)
