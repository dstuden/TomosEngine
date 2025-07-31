#pragma once
#include <typeindex>

#include "TMeshComponent.hh"
#include "Tomos/systems/TSystem.hh"
#include "Tomos/util/renderer/TRenderer.hh"

namespace Tomos
{
    class TMeshSystem : public TSystem
    {
    public:
        TMeshSystem() = default;

        std::type_index getComponentType() const override { return typeid( TMeshComponent ); }

        void componentAdded( const std::shared_ptr<TComponent>& p_component, const std::shared_ptr<TNode>& p_node ) override;
        void componentRemoved( const std::shared_ptr<TComponent>& p_component, const std::shared_ptr<TNode>& p_node ) override;

        void lateUpdate( const std::string& p_layerId ) override;

        void                         clearDrawCalls( const std::string& layerId );
        const std::vector<DrawCall>& getDrawCalls( const std::string& layerId ) const;

    private:
        std::unordered_map<std::string, std::vector<DrawCall>> m_drawCalls;
    };
}  // namespace Tomos
