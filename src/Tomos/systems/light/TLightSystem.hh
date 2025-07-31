#pragma once

#include "../TSystem.hh"
#include "TLightComponent.hh"
#include "Tomos/core/TApplication.hh"
#include "Tomos/util/renderer/TBuffer.hh"

namespace Tomos
{
        struct LightData
        {
            glm::vec3 m_position;
            float     pad0;  // 4 bytes padding
            glm::vec3 m_direction;
            float     pad1;  // 4 bytes padding
            glm::vec3 m_color;
            int       m_type;
            float     m_intensity;
            float     m_range;
            float     m_innerAngle;
            float     m_outerAngle;
        };

    class TLightSystem : public TSystem
    {
    public:
        TLightSystem();
        ~TLightSystem() override;

        void componentAdded( const std::shared_ptr<TComponent>& p_component, const std::shared_ptr<TNode>& p_node ) override;
        void componentRemoved( const std::shared_ptr<TComponent>& p_component, const std::shared_ptr<TNode>& p_node ) override;

        void lateUpdate( const std::string& p_layerId ) override;

        std::type_index getComponentType() const override { return typeid( TLightComponent ); }

        const std::shared_ptr<TStorageBuffer>& getLightsBuffer( const std::string& p_layerId ) const { return m_lightsBuffers.at( p_layerId ); }

    private:
        std::unordered_map<std::string, std::shared_ptr<TStorageBuffer>> m_lightsBuffers;
    };
}  // namespace Tomos
