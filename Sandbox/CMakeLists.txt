cmake_minimum_required(VERSION 3.28)

project(SandBox)

add_executable(${PROJECT_NAME}
    main.cc
)

# The Tomos target exports its src/ directory as a PUBLIC include path, so
# linking against it is sufficient — no manual target_include_directories needed.
target_link_libraries(${PROJECT_NAME} PRIVATE Tomos tomos_build_flags)
if(TARGET TomosEditor)
	target_link_libraries(${PROJECT_NAME} PRIVATE TomosEditor)
endif()

tomos_enable_ipo(${PROJECT_NAME})

# ---------------------------------------------------------------------------
# Copy the assets/ directory next to the built binary so that paths like
# "assets/sponza.glb" resolve correctly at runtime.
# ---------------------------------------------------------------------------
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/assets
        $<TARGET_FILE_DIR:${PROJECT_NAME}>/assets
    COMMENT "Copying assets to build directory"
)

# Engine branding (window icon, etc.) — TomosEngine/resources/Logo.png
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/TomosEngine/resources
        $<TARGET_FILE_DIR:${PROJECT_NAME}>/resources
    COMMENT "Copying Tomos resources to build directory"
)

# ---------------------------------------------------------------------------
# Copy compiled SPIR-V shaders (produced by TomosEngine) next to the binary.
# ---------------------------------------------------------------------------
add_dependencies(${PROJECT_NAME} TomosShaders)
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_BINARY_DIR}/TomosEngine/shaders
        $<TARGET_FILE_DIR:${PROJECT_NAME}>/shaders
    COMMENT "Copying shaders to build directory"
)
