cmake_minimum_required(VERSION 3.28)
project(Tomos)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(TomosOptions)
include(TomosCompilerFlags)

add_subdirectory(TomosEngine)
add_subdirectory(Sandbox)

# Minimal install layout (binary + runtime shaders/assets/resources).
install(TARGETS SandBox RUNTIME DESTINATION bin)
install(DIRECTORY ${CMAKE_BINARY_DIR}/TomosEngine/shaders/
	DESTINATION share/tomos/shaders
	OPTIONAL)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/Sandbox/assets/
	DESTINATION share/tomos/assets
	OPTIONAL)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/TomosEngine/resources/
	DESTINATION share/tomos/resources
	OPTIONAL)
