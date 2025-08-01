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
        src/Tomos/util/logger/TLogger.cc
        src/Tomos/util/logger/TLogger.hh
        src/Tomos/core/TWindow.cc
        src/Tomos/core/TWindow.hh
        src/Tomos/events/TEvent.cc
        src/Tomos/events/TEvent.hh
        src/Tomos/events/key/TKeyEvent.cc
        src/Tomos/events/key/TKeyEvent.hh
        src/Tomos/events/mouse/TMouseEvent.cc
        src/Tomos/events/mouse/TMouseEvent.hh
        src/Tomos/events/application/TApplicationEvent.cc
        src/Tomos/events/application/TApplicationEvent.hh
        src/Tomos/core/TApplication.cc
        src/Tomos/core/TApplication.hh
        src/Tomos/util/renderer/TShader.cc
        src/Tomos/util/renderer/TShader.hh
        src/Tomos/core/TNode.cc
        src/Tomos/core/TNode.hh
        src/Tomos/lib/stb_image.h
        src/Tomos/core/TEcs.cc
        src/Tomos/core/TEcs.hh
        src/Tomos/systems/TSystem.hh
        src/Tomos/systems/TComponent.hh
        src/Tomos/util/transform/TTransform.cc
        src/Tomos/util/transform/TTransform.hh
        src/Tomos/systems/camera/TCameraComponent.cc
        src/Tomos/systems/camera/TCameraComponent.hh
        src/Tomos/systems/camera/TCameraSystem.cc
        src/Tomos/systems/camera/TCameraSystem.hh
        src/Tomos/util/input/TInput.cc
        src/Tomos/util/input/TInput.hh
        src/Tomos/core/TLayer.cc
        src/Tomos/core/TLayer.hh
        src/Tomos/util/renderer/TBuffer.cc
        src/Tomos/util/renderer/TBuffer.hh
        src/Tomos/util/renderer/TVertexArray.cc
        src/Tomos/util/renderer/TVertexArray.hh
        src/Tomos/util/renderer/TRenderer.cc
        src/Tomos/util/renderer/TRenderer.hh
        src/Tomos/util/time/TTime.hh
        src/Tomos/util/resourceManager/TResourceManager.hh
        src/Tomos/systems/script/TScriptSystem.cc
        src/Tomos/systems/script/TScriptSystem.hh
        src/Tomos/systems/script/TScriptComponent.hh
        src/Tomos/lib/lib.cc
        src/Tomos/lib/json.hpp
        src/Tomos/systems/mesh/TMeshComponent.hh
        src/Tomos/systems/mesh/TMeshSystem.cc
        src/Tomos/systems/mesh/TMeshSystem.hh
        src/Tomos/systems/mesh/TMesh.cc
        src/Tomos/systems/mesh/TMesh.hh
        src/Tomos/util/gltfLoader/TGLBLoader.cc
        src/Tomos/util/gltfLoader/TGLBLoader.hh
        src/Tomos/util/renderer/TTexture.cc
        src/Tomos/util/renderer/TTexture.hh
        src/Tomos/util/renderer/TSampler.cc
        src/Tomos/util/renderer/TSampler.hh
        src/Tomos/systems/physics/TPhysicsSystem.cc
        src/Tomos/systems/physics/TPhysicsSystem.hh
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
        src/Tomos/util/renderer/TMaterial.cc
        src/Tomos/util/renderer/TMaterial.hh
        src/Tomos/util/renderer/TFrameBuffer.cc
        src/Tomos/util/renderer/TFrameBuffer.hh
        src/Tomos/core/TScene.cc
        src/Tomos/core/TScene.hh
        src/Tomos/util/conf/TConfig.hh
        src/Tomos/util/renderer/passes/TRenderPass.hh
        src/Tomos/util/resourceManager/TResourceManager.cc
        src/Tomos/util/renderer/passes/gbuff/TGBufferPass.cc
        src/Tomos/util/renderer/passes/gbuff/TGBufferPass.hh
        src/Tomos/systems/light/TLightComponent.hh
        src/Tomos/systems/light/TLightSystem.cc
        src/Tomos/systems/light/TLightSystem.hh
        src/Tomos/util/renderer/passes/light/TLightPass.cc
        src/Tomos/util/renderer/passes/light/TLightPass.hh
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
