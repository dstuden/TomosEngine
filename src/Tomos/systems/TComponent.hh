#pragma once

#include <string>

namespace Tomos
{
    class TComponent
    {
    public:
        virtual ~TComponent() = default;

        std::string m_name{};
    };
} // namespace Tomos
