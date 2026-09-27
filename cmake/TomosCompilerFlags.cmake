# Shared Release / RelWithDebInfo compile & link opts for Tomos targets.

add_library(tomos_build_flags INTERFACE)

# C++26 P2996 reflection (std::meta). Mainline: GCC 16+ with -freflection.
# Upstream Clang does not ship reflection yet (Bloomberg clang-p2996 fork only).
# Linked PUBLIC on Tomos so dependents compiling headers that use <meta> get the flag.
add_library(tomos_reflection_flags INTERFACE)
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
	if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 16)
		message(FATAL_ERROR "Tomos requires GCC 16+ for C++26 reflection (found ${CMAKE_CXX_COMPILER_VERSION})")
	endif()
	target_compile_options(tomos_reflection_flags INTERFACE -freflection)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
	message(WARNING
		"Tomos uses C++26 reflection (-freflection). Upstream Clang does not support it yet; "
		"use GCC 16+ or the Bloomberg clang-p2996 fork with -freflection-latest.")
endif()

# Extra CPU opts for local Release / RelWithDebInfo builds (GCC/Clang; skip MSVC).
# --gc-sections is GNU ld / ELF-oriented; skip on MSVC and Apple ld.
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
	target_compile_options(tomos_build_flags INTERFACE
		$<$<OR:$<CONFIG:Release>,$<CONFIG:RelWithDebInfo>>:-O3 -ffunction-sections -fdata-sections>
	)
	if(NOT APPLE)
		target_link_options(tomos_build_flags INTERFACE
			$<$<OR:$<CONFIG:Release>,$<CONFIG:RelWithDebInfo>>:-Wl,--gc-sections>
		)
	endif()
endif()

include(CheckIPOSupported)
check_ipo_supported(RESULT TOMOS_IPO_SUPPORTED OUTPUT TOMOS_IPO_ERROR)
if(NOT TOMOS_IPO_SUPPORTED)
	message(STATUS "IPO/LTO not available for Release: ${TOMOS_IPO_ERROR}")
endif()

# Enable LTO on Release for a target when the toolchain supports it.
function(tomos_enable_ipo target)
	if(TOMOS_IPO_SUPPORTED)
		set_property(TARGET ${target} PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
	endif()
endfunction()
