#include "Tomos/systems/script/TScriptRegistry.hh"

#include "Tomos/util/logger/TLogger.hh"

namespace Tomos
{
    TScriptRegistry& TScriptRegistry::get()
    {
        static TScriptRegistry sInstance;
        return sInstance;
    }

    void TScriptRegistry::registerType( const std::string& p_name, Factory p_factory )
    {
        if ( p_name.empty() || !p_factory )
        {
            TLOG_WARN() << "[TScriptRegistry] Ignoring empty name or null factory";
            return;
        }
        if ( m_factories.contains( p_name ) )
            TLOG_WARN() << "[TScriptRegistry] Replacing script type '" << p_name << "'";
        else
            m_names.push_back( p_name );
        m_factories[ p_name ] = std::move( p_factory );
    }

    bool TScriptRegistry::has( const std::string& p_name ) const { return m_factories.contains( p_name ); }

    std::unique_ptr<TScript> TScriptRegistry::create( const std::string& p_name ) const
    {
        const auto it = m_factories.find( p_name );
        if ( it == m_factories.end() )
        {
            TLOG_WARN() << "[TScriptRegistry] Unknown script type '" << p_name << "'";
            return nullptr;
        }
        return it->second();
    }
}  // namespace Tomos
