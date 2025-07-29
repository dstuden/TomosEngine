#pragma once
#include <typeindex>

#include "MeshComponent.hh"
#include "Tomos/systems/System.hh"
#include "Tomos/util/renderer/Renderer.hh"

namespace Tomos
{
    class MeshSystem : public System
    {
    public:
        MeshSystem() = default;

        std::type_index getComponentType() const override { return typeid( MeshComponent ); }

        void componentAdded( const std::shared_ptr<Component>& p_component, const std::shared_ptr<Node>& p_node ) override;
        void componentRemoved( const std::shared_ptr<Component>& p_component, const std::shared_ptr<Node>& p_node ) override;

        void lateUpdate( const std::string& p_layerId ) override;

        void                         clearDrawCalls( const std::string& layerId );
        const std::vector<DrawCall>& getDrawCalls( const std::string& layerId ) const;

    private:
        std::unordered_map<std::string, std::vector<DrawCall>> m_drawCalls;
    };
}  // namespace Tomos
