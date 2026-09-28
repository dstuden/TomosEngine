#pragma once

namespace Tomos
{
    class TComponent
    {
    public:
        TComponent()          = default;
        virtual ~TComponent() = default;

        TComponent( const TComponent& )            = delete;
        TComponent& operator=( const TComponent& ) = delete;
        TComponent( TComponent&& )                 = default;
        TComponent& operator=( TComponent&& )      = default;
    };
}  // namespace Tomos
