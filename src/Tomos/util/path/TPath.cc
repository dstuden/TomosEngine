#include "Tomos/util/path/TPath.hh"

#include <vector>

#if defined( __linux__ )
#include <climits>
#include <unistd.h>
#elif defined( _WIN32 )
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#elif defined( __APPLE__ )
#include <mach-o/dyld.h>
#endif

namespace Tomos
{
    namespace
    {
        bool looksLikeAssetRoot( const std::filesystem::path& p_dir, const std::filesystem::path& p_configName )
        {
            std::error_code ec;
            if ( std::filesystem::is_directory( p_dir / "assets", ec ) ) return true;
            if ( !p_configName.empty() && std::filesystem::exists( p_dir / p_configName, ec ) ) return true;
            return false;
        }

        std::filesystem::path normalizeDir( const std::filesystem::path& p_dir )
        {
            std::error_code ec;
            auto            canonical = std::filesystem::weakly_canonical( p_dir, ec );
            if ( !ec ) return canonical;
            return p_dir;
        }
    }  // namespace

    std::filesystem::path& TPath::rootStorage()
    {
        static std::filesystem::path sRoot = std::filesystem::current_path();
        return sRoot;
    }

    std::string& TPath::configPathArgStorage()
    {
        static std::string sArg = "tomos.json";
        return sArg;
    }

    std::filesystem::path& TPath::configPathStorage()
    {
        static std::filesystem::path sPath;
        return sPath;
    }

    std::filesystem::path TPath::executableDir()
    {
#if defined( __linux__ )
        char    buf[ PATH_MAX ];
        ssize_t n = ::readlink( "/proc/self/exe", buf, sizeof( buf ) - 1 );
        if ( n > 0 )
        {
            buf[ n ] = '\0';
            return normalizeDir( std::filesystem::path( buf ).parent_path() );
        }
#elif defined( _WIN32 )
        char  buf[ MAX_PATH ];
        DWORD n = GetModuleFileNameA( nullptr, buf, MAX_PATH );
        if ( n > 0 && n < MAX_PATH ) return normalizeDir( std::filesystem::path( buf ).parent_path() );
#elif defined( __APPLE__ )
        char     buf[ PATH_MAX ];
        uint32_t size = sizeof( buf );
        if ( _NSGetExecutablePath( buf, &size ) == 0 ) return normalizeDir( std::filesystem::path( buf ).parent_path() );
#endif
        return normalizeDir( std::filesystem::current_path() );
    }

    const std::filesystem::path& TPath::assetRoot() { return rootStorage(); }

    void TPath::setAssetRoot( const std::filesystem::path& p_root ) { rootStorage() = normalizeDir( p_root ); }

    void TPath::setConfigPath( const std::string& p_path )
    {
        if ( !p_path.empty() ) configPathArgStorage() = p_path;
    }

    const std::string& TPath::configPathArg() { return configPathArgStorage(); }

    const std::filesystem::path& TPath::configPath() { return configPathStorage(); }

    std::filesystem::path TPath::configDir()
    {
        const auto& path = configPathStorage();
        if ( path.empty() ) return {};
        return path.parent_path();
    }

    void TPath::init( const std::string& p_configPath )
    {
        if ( !p_configPath.empty() ) setConfigPath( p_configPath );

        const std::filesystem::path config( configPathArg() );
        const std::filesystem::path configName = config.filename();

        std::vector<std::filesystem::path> candidates;
        candidates.reserve( 4 );

        std::error_code ec;
        if ( config.is_absolute() )
        {
            if ( std::filesystem::exists( config, ec ) )
                candidates.push_back( config.parent_path() );
            else if ( !config.parent_path().empty() )
                candidates.push_back( config.parent_path() );
        }
        else if ( std::filesystem::exists( config, ec ) )
        {
            candidates.push_back( std::filesystem::current_path() );
        }

        candidates.push_back( executableDir() );
        candidates.push_back( std::filesystem::current_path() );

        for ( const auto& candidate : candidates )
        {
            if ( candidate.empty() ) continue;
            if ( looksLikeAssetRoot( candidate, configName ) )
            {
                setAssetRoot( candidate );
                configPathStorage() = resolve( configPathArg() );
                return;
            }
        }

        setAssetRoot( std::filesystem::current_path() );
        configPathStorage() = resolve( configPathArg() );
    }

    std::filesystem::path TPath::resolve( const std::filesystem::path& p_path )
    {
        if ( p_path.empty() ) return p_path;
        if ( p_path.is_absolute() ) return normalizeDir( p_path );
        return normalizeDir( assetRoot() / p_path );
    }

    std::string TPath::resolveString( const std::string& p_path ) { return resolve( p_path ).string(); }
}  // namespace Tomos
