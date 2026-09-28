#pragma once

#include <filesystem>
#include <string>

namespace Tomos
{
    // Relative paths resolve against assetRoot() (discovered at init).
    class TPath
    {
    public:
        [[nodiscard]] static std::filesystem::path executableDir();

        [[nodiscard]] static const std::filesystem::path& assetRoot();
        static void                                       setAssetRoot( const std::filesystem::path& p_root );

        // Before init (optional): game-preferred relative/absolute config file path.
        static void                     setConfigPath( const std::string& p_path );
        [[nodiscard]] static const std::string& configPathArg();

        // After init:
        [[nodiscard]] static const std::filesystem::path& configPath();
        [[nodiscard]] static std::filesystem::path        configDir();

        // Uses configPathArg() when p_configPath is empty; otherwise setConfigPath then discover root.
        static void init( const std::string& p_configPath = {} );

        [[nodiscard]] static std::filesystem::path resolve( const std::filesystem::path& p_path );
        [[nodiscard]] static std::string           resolveString( const std::string& p_path );

    private:
        static std::filesystem::path& rootStorage();
        static std::string&           configPathArgStorage();
        static std::filesystem::path& configPathStorage();
    };
}  // namespace Tomos
