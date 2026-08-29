#include "Tomos/util/shader/TShaderHotReload.hh"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <fstream>

#include "Tomos/core/input/TInput.hh"
#include "Tomos/util/logger/TLogger.hh"
#include "Tomos/util/path/TPath.hh"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

namespace Tomos
{
    namespace
    {
        constexpr auto g_kPollInterval = std::chrono::milliseconds( 500 );

        const std::array<const char*, 22> g_kGlslFiles = {
                "forward.vert",       "forward.frag",      "shadow.vert",          "skinned.vert",  "skinned_shadow.vert", "cluster_cull.comp", "sprite.vert",
                "sprite.frag",        "particle_sim.comp", "particle.vert",        "particle.frag", "fullscreen.vert",     "tonemap.frag",      "fog.frag",
                "bloom_extract.frag", "bloom_blur.frag",   "bloom_composite.frag", "sao_linearize.frag", "sao_sample.frag", "sao_blur.frag",
                "sao_temporal.frag",  "sao_compose.frag",
        };

        std::filesystem::file_time_type fileMtime( const std::filesystem::path& p_path )
        {
            std::error_code ec;
            auto            t = std::filesystem::last_write_time( p_path, ec );
            return ec ? std::filesystem::file_time_type{} : t;
        }
    }  // namespace

    std::filesystem::path TShaderHotReload::shaderSrcDir()
    {
#ifdef TOMOS_SHADER_SRC_DIR
        return std::filesystem::path( TOMOS_SHADER_SRC_DIR );
#else
        return TPath::executableDir() / ".." / ".." / "TomosEngine" / "src" / "Tomos" / "gpu" / "vulkan" / "shaders";
#endif
    }

    std::filesystem::path TShaderHotReload::shaderSpvDir()
    {
        const std::filesystem::path local = std::filesystem::path( "shaders" );
        std::error_code             ec;
        if ( std::filesystem::is_directory( local, ec ) ) return local;

#ifdef TOMOS_SHADER_DIR
        return std::filesystem::path( TOMOS_SHADER_DIR );
#else
        return local;
#endif
    }

    std::string TShaderHotReload::glslcPath()
    {
#ifdef TOMOS_GLSLC
        return TOMOS_GLSLC;
#else
        return "glslc";
#endif
    }

    void TShaderHotReload::init()
    {
#ifndef TOMOS_DEBUG
        m_inited = true;
        return;
#else
        refreshWatchList();
        m_inited   = true;
        m_lastPoll = std::chrono::steady_clock::now();
        TLOG_INFO() << "[TShaderHotReload] Watching " << m_sources.size() << " GLSL files under " << shaderSrcDir() << " (F5 or save to reload)";
#endif
    }

    void TShaderHotReload::refreshWatchList()
    {
        m_sources.clear();
        m_spvMtimes.clear();

        const auto srcDir = shaderSrcDir();
        const auto spvDir = shaderSpvDir();

        std::error_code ec;
        if ( std::filesystem::is_directory( srcDir / "common", ec ) )
        {
            for ( const auto& entry : std::filesystem::directory_iterator( srcDir / "common", ec ) )
            {
                if ( !entry.is_regular_file() ) continue;
                if ( entry.path().extension() != ".glsl" ) continue;
                m_sources.push_back( { entry.path(), fileMtime( entry.path() ), true } );
            }
        }

        for ( const char* name : g_kGlslFiles )
        {
            const auto path = srcDir / name;
            if ( !std::filesystem::exists( path, ec ) ) continue;
            m_sources.push_back( { path, fileMtime( path ), false } );

            const auto spv      = spvDir / ( std::string( name ) + ".spv" );
            m_spvMtimes[ name ] = fileMtime( spv );
        }
    }

    bool TShaderHotReload::compileOne( const std::filesystem::path& p_glsl, const std::filesystem::path& p_spvOut )
    {
        std::error_code ec;
        std::filesystem::create_directories( p_spvOut.parent_path(), ec );

        const std::string cmd = glslcPath() + " -I\"" + shaderSrcDir().string() + "\" -o \"" + p_spvOut.string() + "\" \"" + p_glsl.string() + "\"";
        TLOG_INFO() << "[TShaderHotReload] " << cmd;
        const int rc = std::system( cmd.c_str() );
        if ( rc != 0 )
        {
            TLOG_ERROR() << "[TShaderHotReload] glslc failed for " << p_glsl.filename().string() << " (exit " << rc << ")";
            return false;
        }

        // Keep the build-tree SPV in sync when we wrote next to the binary.
#ifdef TOMOS_SHADER_DIR
        const auto buildSpv = std::filesystem::path( TOMOS_SHADER_DIR ) / p_spvOut.filename();
        if ( buildSpv != p_spvOut )
        {
            std::filesystem::create_directories( buildSpv.parent_path(), ec );
            std::filesystem::copy_file( p_spvOut, buildSpv, std::filesystem::copy_options::overwrite_existing, ec );
        }
#endif
        return true;
    }

    bool TShaderHotReload::recompileDirty( bool p_all )
    {
        const auto spvDir = shaderSpvDir();

        bool headerDirty = false;
        for ( auto& entry : m_sources )
        {
            if ( !entry.m_isHeader ) continue;
            const auto now = fileMtime( entry.m_path );
            if ( now != entry.m_mtime )
            {
                entry.m_mtime = now;
                headerDirty   = true;
            }
        }

        bool anyOk    = false;
        bool anyFail  = false;
        bool anyDirty = headerDirty || p_all;

        for ( auto& entry : m_sources )
        {
            if ( entry.m_isHeader ) continue;

            const auto now   = fileMtime( entry.m_path );
            const bool dirty = p_all || headerDirty || now != entry.m_mtime;
            entry.m_mtime    = now;
            if ( !dirty ) continue;

            anyDirty           = true;
            const auto spvName = entry.m_path.filename().string() + ".spv";
            const auto spvPath = spvDir / spvName;
            if ( compileOne( entry.m_path, spvPath ) )
            {
                anyOk                                           = true;
                m_spvMtimes[ entry.m_path.filename().string() ] = fileMtime( spvPath );
            }
            else
            {
                anyFail = true;
            }
        }

        // Also pick up externally rebuilt SPVs (e.g. cmake --build TomosShaders).
        for ( const char* name : g_kGlslFiles )
        {
            const auto spv  = spvDir / ( std::string( name ) + ".spv" );
            const auto now  = fileMtime( spv );
            auto       it   = m_spvMtimes.find( name );
            const auto prev = ( it != m_spvMtimes.end() ) ? it->second : std::filesystem::file_time_type{};
            if ( now != prev && now != std::filesystem::file_time_type{} )
            {
                m_spvMtimes[ name ] = now;
                anyDirty            = true;
                anyOk               = true;
            }
        }

        if ( !anyDirty ) return false;
        if ( anyFail && !anyOk )
        {
            TLOG_WARN() << "[TShaderHotReload] Compile failed — keeping previous pipelines";
            return false;
        }
        return true;
    }

    bool TShaderHotReload::tick( GLFWwindow* p_window )
    {
#ifndef TOMOS_DEBUG
        ( void ) p_window;
        return false;
#else
        if ( !m_inited ) init();

        const bool force = m_force || TInput::keyPressed( p_window, GLFW_KEY_F5, m_f5WasDown );
        m_force          = false;

        const auto now = std::chrono::steady_clock::now();
        if ( !force && ( now - m_lastPoll ) < g_kPollInterval ) return false;
        m_lastPoll = now;

        if ( m_sources.empty() ) refreshWatchList();

        if ( !recompileDirty( force ) ) return false;

        TLOG_INFO() << "[TShaderHotReload] Reloading GPU pipelines";
        return true;
#endif
    }
}  // namespace Tomos
