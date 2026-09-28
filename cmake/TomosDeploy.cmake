# Public helpers for compiling GLSL and staging runtime data next to an app binary.
# Used by the engine (TomosShaders) and by games alike.

find_program(TOMOS_GLSLC glslc HINTS ${Vulkan_GLSLC_EXECUTABLE})
if(NOT TOMOS_GLSLC)
	message(FATAL_ERROR
		"glslc not found. Install the Vulkan SDK or a shaderc package and ensure glslc is on PATH.\n"
		"  Arch:     pacman -S shaderc\n"
		"  Debian:   apt install glslc\n"
		"  macOS:    brew install shaderc  (or LunarG Vulkan SDK)\n"
		"  Windows:  install LunarG Vulkan SDK and add its Bin directory to PATH")
endif()

# Compile GLSL → SPIR-V. Creates custom target <name> and sets:
#   TOMOS_SHADER_OUTPUT_DIR  — directory of *.spv
#   TOMOS_SHADER_BASENAMES   — list of GLSL basenames (e.g. forward.frag)
#
# tomos_compile_shaders(<name>
#   SOURCES <glsl>...
#   [INCLUDE_DIRS <dir>...]
#   [OUTPUT_DIR <dir>]
# )
#
# Fails configure if:
#   - duplicate basenames within this target
#   - any basename matches TomosShaders (when <name> is not TomosShaders)
function(tomos_compile_shaders NAME)
	cmake_parse_arguments(ARG "" "OUTPUT_DIR" "SOURCES;INCLUDE_DIRS" ${ARGN})
	if(NOT ARG_SOURCES)
		message(FATAL_ERROR "tomos_compile_shaders(${NAME}): SOURCES is required")
	endif()

	# Basenames for this target + internal duplicate check.
	set(_basenames "")
	set(_seen "")
	foreach(_glsl IN LISTS ARG_SOURCES)
		get_filename_component(_file_name "${_glsl}" NAME)
		list(FIND _seen "${_file_name}" _dup)
		if(NOT _dup EQUAL -1)
			message(FATAL_ERROR
				"tomos_compile_shaders(${NAME}): duplicate shader basename '${_file_name}'\n"
				"  (SPIR-V is flat: shaders/${_file_name}.spv — rename one of the sources)")
		endif()
		list(APPEND _seen "${_file_name}")
		list(APPEND _basenames "${_file_name}")
	endforeach()

	# Collision with engine shaders (games / extra targets only).
	if(NOT NAME STREQUAL "TomosShaders" AND TARGET TomosShaders)
		get_property(_engine_names TARGET TomosShaders PROPERTY TOMOS_SHADER_BASENAMES)
		foreach(_bn IN LISTS _basenames)
			list(FIND _engine_names "${_bn}" _hit)
			if(NOT _hit EQUAL -1)
				message(FATAL_ERROR
					"tomos_compile_shaders(${NAME}): '${_bn}' collides with an engine shader\n"
					"  Engine and game SPIR-V share shaders/<name>.spv — rename the game shader "
					"(e.g. mygame_${_bn}).")
			endif()
		endforeach()
	endif()

	if(ARG_OUTPUT_DIR)
		set(_out_dir "${ARG_OUTPUT_DIR}")
	else()
		set(_out_dir "${CMAKE_CURRENT_BINARY_DIR}/shaders_${NAME}")
	endif()
	file(MAKE_DIRECTORY "${_out_dir}")

	set(_include_dirs ${ARG_INCLUDE_DIRS})
	if(TARGET Tomos)
		get_property(_engine_inc TARGET Tomos PROPERTY TOMOS_SHADER_INCLUDE_DIR)
		if(_engine_inc)
			list(APPEND _include_dirs "${_engine_inc}")
		endif()
	endif()
	list(REMOVE_DUPLICATES _include_dirs)

	set(_base_flags "")
	foreach(_inc IN LISTS _include_dirs)
		list(APPEND _base_flags "-I${_inc}")
	endforeach()

	set(_common_headers "")
	foreach(_inc IN LISTS _include_dirs)
		file(GLOB _hdrs CONFIGURE_DEPENDS "${_inc}/common/*.glsl")
		list(APPEND _common_headers ${_hdrs})
	endforeach()

	set(_spv_outputs "")
	foreach(_glsl IN LISTS ARG_SOURCES)
		get_filename_component(_file_name "${_glsl}" NAME)
		set(_spv "${_out_dir}/${_file_name}.spv")
		set(_opt_flags "")
		if(CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
			list(APPEND _opt_flags -O)
		endif()
		add_custom_command(
			OUTPUT "${_spv}"
			COMMAND ${TOMOS_GLSLC} ${_opt_flags} ${_base_flags} -o "${_spv}" "${_glsl}"
			DEPENDS "${_glsl}" ${_common_headers}
			COMMENT "Compiling ${_file_name} → ${_file_name}.spv"
			VERBATIM
		)
		list(APPEND _spv_outputs "${_spv}")
	endforeach()

	add_custom_target(${NAME} ALL DEPENDS ${_spv_outputs})
	set_property(TARGET ${NAME} PROPERTY TOMOS_SHADER_OUTPUT_DIR "${_out_dir}")
	set_property(TARGET ${NAME} PROPERTY TOMOS_SHADER_BASENAMES "${_basenames}")
endfunction()

# Stage engine (+ optional game) runtime data next to an executable.
#
# tomos_deploy_runtime(<app>
#   [ASSETS <dir>...]
#   [RESOURCES <dir>...]
#   [SHADERS <shader-target>...]   # from tomos_compile_shaders
# )
#
# Fails configure if SHADERS targets share a basename with each other or TomosShaders.
function(tomos_deploy_runtime APP)
	cmake_parse_arguments(ARG "" "" "ASSETS;RESOURCES;SHADERS" ${ARGN})

	if(NOT TARGET ${APP})
		message(FATAL_ERROR "tomos_deploy_runtime: target '${APP}' does not exist")
	endif()
	if(NOT TARGET TomosShaders)
		message(FATAL_ERROR "tomos_deploy_runtime: TomosShaders target missing (add_subdirectory Tomos first)")
	endif()
	if(NOT TARGET Tomos)
		message(FATAL_ERROR "tomos_deploy_runtime: Tomos target missing")
	endif()

	set(_shader_targets ${ARG_SHADERS})

	# Cross-check basenames across all game shader targets (and vs engine).
	get_property(_claimed TARGET TomosShaders PROPERTY TOMOS_SHADER_BASENAMES)
	set(_owners "")
	foreach(_bn IN LISTS _claimed)
		list(APPEND _owners "TomosShaders")
	endforeach()

	list(REMOVE_DUPLICATES _shader_targets)
	foreach(_st IN LISTS _shader_targets)
		if(NOT TARGET ${_st})
			message(FATAL_ERROR "tomos_deploy_runtime: SHADERS target '${_st}' does not exist")
		endif()
		get_property(_names TARGET ${_st} PROPERTY TOMOS_SHADER_BASENAMES)
		foreach(_bn IN LISTS _names)
			list(FIND _claimed "${_bn}" _hit)
			if(NOT _hit EQUAL -1)
				list(GET _owners ${_hit} _prev)
				message(FATAL_ERROR
					"tomos_deploy_runtime(${APP}): shader basename '${_bn}' used by both '${_prev}' and '${_st}'\n"
					"  SPIR-V is flat under shaders/ — rename one of them.")
			endif()
			list(APPEND _claimed "${_bn}")
			list(APPEND _owners "${_st}")
		endforeach()
	endforeach()

	get_property(_engine_shader_dir TARGET TomosShaders PROPERTY TOMOS_SHADER_OUTPUT_DIR)
	get_property(_engine_resources TARGET Tomos PROPERTY TOMOS_RESOURCES_DIR)

	set(_all_shader_targets TomosShaders ${_shader_targets})
	list(REMOVE_DUPLICATES _all_shader_targets)
	add_dependencies(${APP} ${_all_shader_targets})

	# Engine SPIR-V → bin/shaders/
	add_custom_command(TARGET ${APP} POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${APP}>/shaders"
		COMMAND ${CMAKE_COMMAND} -E copy_directory
			"${_engine_shader_dir}"
			"$<TARGET_FILE_DIR:${APP}>/shaders"
		COMMENT "Deploying TomosShaders to $<TARGET_FILE_DIR:${APP}>/shaders"
		VERBATIM
	)

	foreach(_st IN LISTS _shader_targets)
		get_property(_dir TARGET ${_st} PROPERTY TOMOS_SHADER_OUTPUT_DIR)
		if(NOT _dir)
			message(FATAL_ERROR "tomos_deploy_runtime: '${_st}' has no TOMOS_SHADER_OUTPUT_DIR (use tomos_compile_shaders)")
		endif()
		add_custom_command(TARGET ${APP} POST_BUILD
			COMMAND ${CMAKE_COMMAND} -E copy_directory
				"${_dir}"
				"$<TARGET_FILE_DIR:${APP}>/shaders"
			COMMENT "Deploying ${_st} shaders to $<TARGET_FILE_DIR:${APP}>/shaders"
			VERBATIM
		)
	endforeach()

	if(_engine_resources)
		add_custom_command(TARGET ${APP} POST_BUILD
			COMMAND ${CMAKE_COMMAND} -E copy_directory
				"${_engine_resources}"
				"$<TARGET_FILE_DIR:${APP}>/resources"
			COMMENT "Deploying Tomos resources to $<TARGET_FILE_DIR:${APP}>/resources"
			VERBATIM
		)
	endif()

	foreach(_res IN LISTS ARG_RESOURCES)
		add_custom_command(TARGET ${APP} POST_BUILD
			COMMAND ${CMAKE_COMMAND} -E copy_directory
				"${_res}"
				"$<TARGET_FILE_DIR:${APP}>/resources"
			COMMENT "Deploying resources ${_res}"
			VERBATIM
		)
	endforeach()

	foreach(_assets IN LISTS ARG_ASSETS)
		add_custom_command(TARGET ${APP} POST_BUILD
			COMMAND ${CMAKE_COMMAND} -E copy_directory
				"${_assets}"
				"$<TARGET_FILE_DIR:${APP}>/assets"
			COMMENT "Deploying assets ${_assets}"
			VERBATIM
		)
	endforeach()
endfunction()
