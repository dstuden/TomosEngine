#pragma once

#include <chrono>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

struct GLFWwindow;

namespace Tomos
{
    // Dev-only (TOMOS_DEBUG). F5 or mtime change → recompile + pipeline recreate signal.
    class TShaderHotReload
    {
    public:
        void init();

        [[nodiscard]] bool tick( GLFWwindow* p_window );

        void requestReload() { m_force = true; }

    private:
        struct TWatchEntry
        {
            std::filesystem::path                  m_path;
            std::filesystem::file_time_type        m_mtime{};
            bool                                   m_isHeader = false;
        };

        void refreshWatchList();
        bool recompileDirty( bool p_all );
        bool compileOne( const std::filesystem::path& p_glsl, const std::filesystem::path& p_spvOut ) const;

        [[nodiscard]] static std::filesystem::path shaderSrcDir();
        [[nodiscard]] static std::filesystem::path shaderSpvDir();
        [[nodiscard]] static std::string           glslcPath();

        std::vector<TWatchEntry>                                 m_sources;
        std::unordered_map<std::string, std::filesystem::file_time_type> m_spvMtimes;
        bool                                                     m_inited   = false;
        bool                                                     m_force    = false;
        bool                                                     m_f5WasDown = false;
        std::chrono::steady_clock::time_point                    m_lastPoll{};
    };
}  // namespace Tomos
