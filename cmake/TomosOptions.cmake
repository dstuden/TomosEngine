# Build / feature options for Tomos.
#
# Compile macros (set on targets, not here):
#   TOMOS_DEBUG=0|1  — Debug config only (validation, debug logs, perf stats)
#   TOMOS_EDITOR=1   — PUBLIC when linking TomosEditor

option(TOMOS_BUILD_EDITOR "Build TomosEditor library (docked scene editor)" ON)

message(STATUS "Tomos options:")
message(STATUS "  TOMOS_BUILD_EDITOR = ${TOMOS_BUILD_EDITOR}")
message(STATUS "  TOMOS_DEBUG        = 1 in Debug, 0 otherwise (compile def)")
message(STATUS "  TOMOS_EDITOR       = 1 when linking TomosEditor")
