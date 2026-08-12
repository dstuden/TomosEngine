#pragma once

#include <fstream>
#include <functional>
#include <map>
#include <memory>
#include <nlohmann/json.hpp>
#include <ranges>
#include <string>
#include <type_traits>
#include <vector>

#include "Tomos/util/logger/TLogger.hh"

namespace Tomos
{
    class TBaseConfig;
    class TEngineConfig;

    struct TConfigField
    {
        std::string                                  name;
        std::string                                  category;
        std::string                                  description;
        std::function<void( const nlohmann::json& )> loadFunc;
        std::function<void( nlohmann::json& )>       saveFunc;
    };

    const inline std::string g_defaultCategory = "General";

    template<typename T>
    class TProperty
    {
    public:
        T value;

        TProperty( TEngineConfig* p_parent, const std::string& p_name, T p_defaultValue, const std::string& p_category = g_defaultCategory,
                   const std::string& p_description = "" ) : value( p_defaultValue )
        {
            registerWith( p_parent, p_name, p_category, p_description );
        }

        operator const T&() const { return value; }

        T& operator=( const T& p_newValue )
        {
            value = p_newValue;
            return value;
        }

        T* operator->() { return &value; }

    private:
        void registerWith( TEngineConfig* p_parent, const std::string& p_name, const std::string& p_cat, const std::string& p_desc );
    };

    class TBaseConfig
    {
        friend class TConfigManager;

    public:
        virtual ~TBaseConfig() = default;

        void addField( const std::string& p_name, TConfigField&& p_field ) { m_fields[ p_name ] = std::move( p_field ); }

        auto getByCategory()
        {
            std::map<std::string, std::vector<TConfigField*>> categorized;
            for ( auto& field : m_fields | std::views::values )
            {
                categorized[ field.category ].push_back( &field );
            }
            return categorized;
        }

    protected:
        void load( const nlohmann::json& p_data )
        {
            for ( auto& field : m_fields | std::views::values )
            {
                field.loadFunc( p_data );
            }
        }

        nlohmann::json serialize() const
        {
            nlohmann::json out = nlohmann::json::object();
            for ( const auto& field : m_fields | std::views::values )
            {
                field.saveFunc( out );
            }
            return out;
        }

    protected:
        std::map<std::string, TConfigField> m_fields;
    };

    class TEngineConfig : public TBaseConfig
    {
    public:
        TProperty<unsigned int> windowWidth{ this, "windowWidth", 1280, "Video", "Width of window" };
        TProperty<unsigned int> windowHeight{ this, "windowHeight", 720, "Video", "Height of window" };
        TProperty<std::string>  windowTitle{ this, "windowTitle", "Tomos Engine", "General", "Window Title" };
        TProperty<bool>         vsync{ this, "vsync", true, "Video", "Enable VSync" };
        TProperty<bool>         fullscreen{ this, "fullscreen", false, "Video", "Start in fullscreen mode" };
    };

    template<typename T>
    void TProperty<T>::registerWith( TEngineConfig* p_parent, const std::string& p_name, const std::string& p_cat, const std::string& p_desc )
    {
        p_parent->addField( p_name, { p_name, p_cat, p_desc,
                                      [ this, p_name ]( const nlohmann::json& j )
                                      {
                                          if ( j.contains( p_name ) ) value = j.at( p_name ).get<T>();
                                      },
                                      [ this, p_name ]( nlohmann::json& j ) { j[ p_name ] = value; } } );
    }

    class TConfigManager
    {
    public:
        template<typename T>
        T& load( const std::string& p_path )
        {
            static_assert( std::is_base_of_v<TEngineConfig, T>, "T must derive from TEngineConfig" );

            m_path           = p_path;
            m_instance       = std::make_unique<T>();
            m_loadedFromFile = false;

            std::ifstream file( p_path );
            if ( file.is_open() )
            {
                try
                {
                    nlohmann::json data = nlohmann::json::parse( file );
                    m_instance->load( data );
                    m_loadedFromFile = true;
                }
                catch ( const std::exception& e )
                {
                    TLOG_ERROR() << "Failed to parse config file: " << e.what();
                }
            }
            return static_cast<T&>( *m_instance );
        }

        void save() const
        {
            if ( !m_instance ) return;
            std::ofstream file( m_path );
            if ( file.is_open() )
            {
                file << m_instance->serialize().dump( 4 );
            }
        }

        template<typename T>
        T& get()
        {
            return static_cast<T&>( *m_instance );
        }

        [[nodiscard]] bool loadedFromFile() const { return m_loadedFromFile; }

    private:
        std::unique_ptr<TBaseConfig> m_instance;
        std::string                  m_path;
        bool                         m_loadedFromFile = false;
    };
}  // namespace Tomos
