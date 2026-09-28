#pragma once

// Public profiling macros. Compile out entirely when TOMOS_DEBUG=0.
// GPU macros take (timestamps, cmd, frameIndex, PassEnumName).

#ifndef TOMOS_DEBUG
#define TOMOS_DEBUG 0
#endif

#if TOMOS_DEBUG

#include "Tomos/util/profile/TFrameProfiler.hh"
#include "Tomos/util/profile/TGpuTimestamps.hh"

#define TOMOS_PROFILE_CONCAT2( a, b ) a##b
#define TOMOS_PROFILE_CONCAT( a, b ) TOMOS_PROFILE_CONCAT2( a, b )

#define TOMOS_PROFILE_SCOPE( name ) ::Tomos::TProfileScope TOMOS_PROFILE_CONCAT( _tomosProf_, __LINE__ )( name )

#define TOMOS_PROFILE_GPU_BEGIN( ts, cmd, frameIndex, pass ) ( ts ).writeBegin( ( cmd ), ( frameIndex ), ::Tomos::TGpuPass::pass )

#define TOMOS_PROFILE_GPU_END( ts, cmd, frameIndex, pass ) ( ts ).writeEnd( ( cmd ), ( frameIndex ), ::Tomos::TGpuPass::pass )

#else

#define TOMOS_PROFILE_SCOPE( name ) ( ( void ) 0 )
#define TOMOS_PROFILE_GPU_BEGIN( ts, cmd, frameIndex, pass ) ( ( void ) 0 )
#define TOMOS_PROFILE_GPU_END( ts, cmd, frameIndex, pass ) ( ( void ) 0 )

#endif
