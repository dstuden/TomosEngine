#include "TLightSystem.hh"

namespace Tomos
{
    TLightSystem::TLightSystem() {}

    TLightSystem::~TLightSystem() = default;

    void TLightSystem::componentAdded( const std::shared_ptr<TComponent>& p_component, const std::shared_ptr<TNode>& p_node )
    {
        if ( m_components.contains( p_node->getLayerId() ) && !m_components[p_node->getLayerId()].erase( p_component ) )
        {
            TLOG_WARN() << "Component moved to new node: " << p_component->m_name;
        }
        m_components[p_node->getLayerId()][p_component] = p_node;
    }

    void TLightSystem::componentRemoved( const std::shared_ptr<TComponent>& p_component, const std::shared_ptr<TNode>& p_node )
    {
        if ( m_components.contains( p_node->getLayerId() ) ) m_components[p_node->getLayerId()].erase( p_component );
    }

    void TLightSystem::lateUpdate( const std::string& p_layerId )
    {
        using namespace glm;
        std::vector<LightData> lights;

        glm::mat4 proj;
        glm::vec4 thing;

        int shadowIndex = 0;

        for ( auto& [component, node] : m_components[p_layerId] )
        {
            auto light = std::dynamic_pointer_cast<TLightComponent>( component );
            if ( light->getLightInfo()->m_intensity < 0.001f ) continue;

            TTransform transform = node->m_transform;
            glm::mat4  viewProj( 1.0f );

            LightData data = {};

            if ( light->getLightInfo()->m_castsShadows && light->getLightInfo()->m_type != LightType::Point )
            {
                if ( light->getLightInfo()->m_type == LightType::Directional )
                {
                    auto info      = std::dynamic_pointer_cast<DirectionalLightInfo>( light->getLightInfo() );
                    int  dirRadius = TApplication::getState().config().get<int>( "directionalLightRadius" );

                    proj              = glm::ortho( -dirRadius, dirRadius, -dirRadius, dirRadius, -dirRadius, dirRadius );
                    data.m_range      = 0.0f;
                    data.m_innerAngle = 0.0f;
                    data.m_outerAngle = 0.0f;
                    data.m_type       = 0;
                }
                else
                {
                    auto info         = std::dynamic_pointer_cast<SpotLightInfo>( light->getLightInfo() );
                    proj              = glm::perspective( info->m_outerAngle * 2, 1.0f, 0.01f, info->m_range );
                    data.m_range      = info->m_range;
                    data.m_innerAngle = info->m_innerAngle;
                    data.m_outerAngle = info->m_outerAngle;
                    data.m_type       = 2;
                }

                viewProj = proj * transform.m_globInvMat;
            }

            if ( light->getLightInfo()->m_type == LightType::Point )
            {
                auto info         = std::dynamic_pointer_cast<PointLightInfo>( light->getLightInfo() );
                data.m_range      = info->m_range;
                data.m_innerAngle = 0.0f;
                data.m_outerAngle = 0.0f;
                data.m_type       = 1;
            }

            glm::vec4 vec( 0.0f, 0.0f, -1.0f, 0.0f );
            thing = transform.m_globMat * vec;

            data.m_position  = transform.m_globMat[3];
            data.m_direction = glm::vec3( thing.x, thing.y, thing.z );
            data.m_color     = light->getLightInfo()->m_color;
            data.m_intensity = light->getLightInfo()->m_intensity;

            lights.push_back( data );
        }

        if ( !m_lightsBuffers.contains( p_layerId ) )
        {
            unsigned int bufferSize = TApplication::getState().config().get<int>( "maxLights" ) * sizeof( LightData ) + sizeof( int );
            m_lightsBuffers.emplace( p_layerId, std::make_shared<TStorageBuffer>( bufferSize, GL_DYNAMIC_DRAW ) );
        }

        if ( !lights.empty() )
        {
            // buffer magija
            void* bufferData = m_lightsBuffers[p_layerId]->map( GL_WRITE_ONLY );

            memcpy( bufferData, lights.data(), lights.size() * sizeof( LightData ) );

            unsigned int lightCountOffset = TApplication::getState().config().get<int>( "maxLights" ) * sizeof( LightData );

            int* lightCountPtr = ( int* ) ( ( char* ) bufferData + lightCountOffset );
            *lightCountPtr     = lights.size();

            m_lightsBuffers[p_layerId]->unmap();
        }
        else
        {
            void*        bufferData       = m_lightsBuffers[p_layerId]->map( GL_WRITE_ONLY );
            unsigned int lightCountOffset = TApplication::getState().config().get<int>( "maxLights" ) * sizeof( LightData );
            int*         lightCountPtr    = ( int* ) ( ( char* ) bufferData + lightCountOffset );
            *lightCountPtr                = 0;
            m_lightsBuffers[p_layerId]->unmap();
        }
    }
}  // namespace Tomos
