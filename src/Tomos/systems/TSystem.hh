#pragma once

#include "Tomos/systems/TComponent.hh"

namespace Tomos
{
    class TSceneNode;

    class TSystem
    {
    public:
        [[nodiscard]] virtual bool matches( const TComponent& p_component ) const = 0;

        virtual void earlyUpdate( float /*p_dt*/ ) {}
        virtual void update( float /*p_dt*/ ) {}
        virtual void lateUpdate( float /*p_dt*/ ) {}

        virtual void componentCreated( TSceneNode& p_node, TComponent& p_component ) {}
        virtual void componentDestroyed( TSceneNode& p_node, TComponent& p_component ) {}

        virtual ~TSystem() = default;
    };

    template<typename T>
    class TTypedSystem : public TSystem
    {
    public:
        [[nodiscard]] bool matches( const TComponent& p_component ) const override { return dynamic_cast<const T*>( &p_component ) != nullptr; }
    };
}  // namespace Tomos
