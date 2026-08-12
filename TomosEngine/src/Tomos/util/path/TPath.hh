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

        static void init( const std::string& p_configPath = "tomos.json" );

        [[nodiscard]] static std::filesystem::path resolve( const std::filesystem::path& p_path );
        [[nodiscard]] static std::string           resolveString( const std::string& p_path );

    private:
        static std::filesystem::path& rootStorage();
    };
}  // namespace Tomos
