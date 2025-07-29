#include "LightSystem.hh"

namespace Tomos
{
    LightSystem::LightSystem() {}

    LightSystem::~LightSystem() = default;

    void LightSystem::componentAdded( const std::shared_ptr<Component>& p_component, const std::shared_ptr<Node>& p_node )
    {
        if ( m_components.contains( p_node->getLayerId() ) && !m_components[p_node->getLayerId()].erase( p_component ) )
        {
            LOG_WARN() << "Component moved to new node: " << p_component->m_name;
        }
        m_components[p_node->getLayerId()][p_component] = p_node;
    }

    void LightSystem::componentRemoved( const std::shared_ptr<Component>& p_component, const std::shared_ptr<Node>& p_node )
    {
        if ( m_components.contains( p_node->getLayerId() ) ) m_components[p_node->getLayerId()].erase( p_component );
    }

    void LightSystem::lateUpdate( const std::string& p_layerId )
    {
        using namespace glm;
        std::vector<LightData> lights;

        glm::mat4 proj;
        glm::vec4 thing;

        int shadowIndex = 0;

        for ( auto& [component, node] : m_components[p_layerId] )
        {
            auto light = std::dynamic_pointer_cast<LightComponent>( component );
            if ( light->getLightInfo()->m_intensity < 0.001f ) continue;

            Transform transform = node->m_transform;
            glm::mat4 viewProj(1.0f);

            LightData data = {};

            if ( light->getLightInfo()->m_castsShadows && light->getLightInfo()->m_type != LightType::Point )
            {

                if ( light->getLightInfo()->m_type == LightType::Directional )
                {
                    auto info      = std::dynamic_pointer_cast<DirectionalLightInfo>( light->getLightInfo() );
                    int  dirRadius = Application::getState().config().get<int>( "directionalLightRadius" );

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
                data.m_type = 1;
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
            m_lightsBuffers.emplace( p_layerId, std::make_shared<StorageBuffer>( Application::getState().config().get<int>( "maxLights" ) * sizeof( LightData ),
                                                                                 GL_DYNAMIC_DRAW ) );
        }

        m_lightsBuffers[p_layerId]->setData( lights.data(), lights.size() * sizeof( LightData ) );
    }
}  // namespace Tomos
