#pragma once

#include "../System.hh"
#include "LightComponent.hh"
#include "Tomos/core/Application.hh"
#include "Tomos/util/renderer/Buffer.hh"

namespace Tomos
{
//    struct alignas( 16 ) LightData
//    {
//        glm::vec3 m_position;  // Offset 0
//        float     _pad0;  // 4 bytes padding (fills the 16 bytes for m_position)
//        glm::vec3 m_direction;  // Offset 16
//        float     _pad1;  // 4 bytes padding (fills the 16 bytes for m_direction)
//        glm::vec3 m_color;  // Offset 32
//        float     _pad2;  // 4 bytes padding (fills the 16 bytes for m_color)
//        int       m_type;  // Offset 48
//        float     m_intensity;  // Offset 52
//        float     m_range;  // Offset 56 (GLSL: maxRange)
//        float     m_innerAngle;  // Offset 60 (GLSL: innerCone)
//        float     m_outerAngle;  // Offset 64 (GLSL: outerCone)
//        int       m_shadowMap;  // Offset 68
//        float     _pad3;  // 4 bytes padding
//        float     _pad4;  // 4 bytes padding (total 8 bytes to align m_vp to 16-byte boundary)
//        glm::mat4 m_vp;  // Offset 80
//    };

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

    class LightSystem : public System
    {
    public:
        LightSystem();
        ~LightSystem() override;

        void componentAdded( const std::shared_ptr<Component>& p_component, const std::shared_ptr<Node>& p_node ) override;
        void componentRemoved( const std::shared_ptr<Component>& p_component, const std::shared_ptr<Node>& p_node ) override;

        void lateUpdate( const std::string& p_layerId ) override;

        std::type_index getComponentType() const override { return typeid( LightComponent ); }

        const std::shared_ptr<StorageBuffer>& getLightsBuffer( const std::string& p_layerId ) const { return m_lightsBuffers.at( p_layerId ); }

    private:
        std::unordered_map<std::string, std::shared_ptr<StorageBuffer>> m_lightsBuffers;
    };
}  // namespace Tomos
