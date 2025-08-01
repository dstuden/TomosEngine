#pragma once

#include <fstream>
#include <iostream>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <variant>
#include <vector>

#include "Tomos/lib/json.hpp"
#include "Tomos/util/logger/TLogger.hh"

using json = nlohmann::json;

namespace Tomos
{
    class TConfigManager;

    // Type alias for all supported JSON/C++ types
    using TConfigValue = std::variant<int,  // JSON number (integer)
                                      unsigned int,  // JSON number (unsigned)
                                      float,  // JSON number (float)
                                      double,  // JSON number (double)
                                      bool,  // JSON boolean
                                      long,  // JSON number (long)
                                      unsigned long,  // JSON number (unsigned long)
                                      std::string,  // JSON string
                                      std::vector<int>,  // JSON array of integers
                                      std::vector<float>,  // JSON array of floats
                                      std::vector<double>,  // JSON array of doubles
                                      std::vector<bool>,  // JSON array of booleans
                                      std::vector<std::string>,  // JSON array of strings
                                      json  // For nested objects or untyped values
                                      >;

    // Base class for configuration schemas
    class TConfigSchema
    {
    public:
        virtual ~TConfigSchema() = default;

        // Register all fields with the ConfigManager
        virtual void registerFields( TConfigManager& p_manager ) = 0;

        // Load values from JSON
        virtual void load( const json& p_data ) = 0;
    };

    // Concrete schema implementation using CRTP
    template<typename T>
    class TConfigSchemaImpl : public TConfigSchema
    {
    public:
        void registerFields( TConfigManager& p_manager ) override { static_cast<T*>( this )->defineFields( p_manager ); }

        void load( const json& p_data ) override { static_cast<T*>( this )->loadFields( p_data ); }
    };

    class TConfigManager
    {
    public:
        // Register a schema and its fields
        template<typename Schema>
        void setSchema()
        {
            bool isBase = std::is_base_of_v<TConfigSchema, Schema>;
            TLOG_ASSERT_MSG( isBase, "Schema must inherit from ConfigSchema" );

            m_currentSchema = std::make_unique<Schema>();
            m_currentSchema->registerFields( *this );
        }

        // Load configuration from file
        bool loadConfig( const std::string& p_configPath )
        {
            TLOG_ASSERT_MSG( m_currentSchema, "No schema set. Call setSchema() first." );

            try
            {
                std::ifstream configFile( p_configPath );
                if ( !configFile.is_open() )
                {
                    TLOG_ERROR() << "Warning: Could not open config file: " << p_configPath << ". Using defaults.";
                    return false;
                }

                json fileData       = json::parse( configFile );
                m_currentConfigPath = p_configPath;

                // Let the schema load the data it recognizes
                m_currentSchema->load( fileData );

                // Store the complete JSON for extended access
                m_configData = fileData;

                return true;
            }
            catch ( const json::parse_error& e )
            {
                TLOG_ERROR() << "JSON parse error: " << e.what();
                return false;
            }
            catch ( const std::exception& e )
            {
                TLOG_ERROR() << "Error loading config: " << e.what();
                return false;
            }
        }

        // Register a field with the manager
        template<typename T>
        void registerField( const std::string& p_key, T* p_fieldPtr, const T& p_defaultValue )
        {
            m_fieldRegistry[p_key] = p_fieldPtr;
            *p_fieldPtr            = p_defaultValue;

            // Store the default value in case we need to reset
            m_defaultValues[p_key] = p_defaultValue;
        }

        // Get a value (type-safe)
        template<typename T>
        T get( const std::string& p_key ) const
        {
            try
            {
                if ( m_configData.contains( p_key ) )
                {
                    return m_configData[p_key].get<T>();
                }

                // Check if we have a default value
                auto it = m_defaultValues.find( p_key );
                if ( it != m_defaultValues.end() )
                {
                    return std::get<T>( it->second );
                }

                throw std::runtime_error( "Key not found: " + p_key );
            }
            catch ( const json::exception& e )
            {
                TLOG_ERROR() << "Type mismatch for key '" + p_key + "': " + e.what();
                throw;
            }
            catch ( const std::exception& e )
            {
                TLOG_ERROR() << "Error getting value for key '" + p_key + "': " + e.what();
                throw;
            }
        }

        // Get raw JSON data
        const json& getRawData() const { return m_configData; }

    private:
        std::unique_ptr<TConfigSchema>                m_currentSchema;
        std::unordered_map<std::string, void*>        m_fieldRegistry;
        std::unordered_map<std::string, TConfigValue> m_defaultValues;
        json                                          m_configData;
        std::string                                   m_currentConfigPath;
    };


    class TBaseConfig : public TConfigSchemaImpl<TBaseConfig>
    {
    public:
        std::string   m_unassignedLayerId      = "UNASSIGNED_LAYER";
        unsigned long m_maxInstancesPerDraw    = 1024;
        int           m_maxShadowsMaps         = 16;
        int           m_maxLights              = 128;
        int           m_directionalLightRadius = 16;
        int           m_maxNodeDepth           = 333;
        std::string   m_logDir                 = "logs/" + std::to_string( std::time( nullptr ) ) + ".log";

        void defineFields( TConfigManager& p_manager )
        {
            p_manager.registerField( "unassignedLayerId", &m_unassignedLayerId, m_unassignedLayerId );
            p_manager.registerField( "maxInstancesPerDraw", &m_maxInstancesPerDraw, m_maxInstancesPerDraw );
            p_manager.registerField( "maxShadowMaps", &m_maxShadowsMaps, m_maxShadowsMaps );
            p_manager.registerField( "maxLights", &m_maxLights, m_maxLights );
            p_manager.registerField( "directionalLightRadius", &m_directionalLightRadius, m_directionalLightRadius );
            p_manager.registerField( "maxNodeDepth", &m_maxNodeDepth, m_maxNodeDepth );
            p_manager.registerField( "logFilePath", &m_logDir, m_logDir );
        }

        void loadFields( const json& p_data )
        {
            if ( p_data.contains( "unassignedLayerId" ) ) m_unassignedLayerId = p_data["unassignedLayerId"].get<std::string>();
            if ( p_data.contains( "maxInstancesPerDraw" ) ) m_maxInstancesPerDraw = p_data["maxInstancesPerDraw"].get<unsigned long>();
            if ( p_data.contains( "maxShadowMaps" ) ) m_maxShadowsMaps = p_data["maxShadowMaps"].get<int>();
            if ( p_data.contains( "maxLights" ) ) m_maxLights = p_data["maxLights"].get<int>();
            if ( p_data.contains( "directionalLightRadius" ) ) m_directionalLightRadius = p_data["directionalLightRadius"].get<int>();
            if ( p_data.contains( "maxNodeDepth" ) ) m_maxNodeDepth = p_data["maxNodeDepth"].get<int>();
            if ( p_data.contains( "logFilePath" ) ) m_logDir = p_data["logFilePath"].get<std::string>();
        }
    };

    // Global instance of the configuration manager
    namespace Global
    {
        inline TConfigManager config;  // NOLINT(*-identifier-naming)
    }
}  // namespace Tomos
