#pragma once

#include <glm/glm.hpp>

#include "../TSystem.hh"
#include "TCameraComponent.hh"

namespace Tomos
{
    class TCameraSystem : public TSystem
    {
    public:
        TCameraSystem() = default;

        void componentAdded( const std::shared_ptr<TComponent>& p_component, const std::shared_ptr<TNode>& p_node ) override;
        void componentRemoved( const std::shared_ptr<TComponent>& p_component, const std::shared_ptr<TNode>& p_node ) override;

        void update( const std::string& p_layerId ) override;

        std::type_index getComponentType() const override { return typeid( TCameraComponent ); }

        std::shared_ptr<TNode> getActiveCameraNode( const std::string& p_layerId );

        const std::shared_ptr<TCameraComponent>& getActiveCamera( const std::string& p_layerId );
        const glm::mat4&                        getViewProjectionMat( const std::string& p_layerId );
        const glm::mat4&                        getViewProjectionInvMat( const std::string& p_layerId );
        const glm::mat4&                        getViewMat( const std::string& p_layerId );
        const glm::mat4&                        getViewInvMat( const std::string& p_layerId );

        struct CameraData
        {
            std::shared_ptr<TCameraComponent> m_activeCamera;
            glm::mat4                        m_viewProjMat;
            glm::mat4                        m_viewProjMatInv;
            glm::mat4                        m_viewMat;
            glm::mat4                        m_viewMatInv;
        };

    private:
        std::unordered_map<std::string, CameraData> m_layerActiveData;
        CameraData                                  m_defaultCameraData;
    };
}  // namespace Tomos
