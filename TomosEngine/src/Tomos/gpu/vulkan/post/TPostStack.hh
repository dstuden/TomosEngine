#pragma once

#include <memory>
#include <vector>

#include "Tomos/gpu/vulkan/post/TPostEffect.hh"

namespace Tomos
{
    class TPostStack
    {
    public:
        TPostStack() = default;
        ~TPostStack();

        TPostStack( const TPostStack& )            = delete;
        TPostStack& operator=( const TPostStack& ) = delete;

        void add( std::unique_ptr<TPostEffect> p_effect );

        template<typename T>
        T* find()
        {
            for ( auto& e : m_effects )
                if ( auto* t = dynamic_cast<T*>( e.get() ) ) return t;
            return nullptr;
        }

        [[nodiscard]] const std::vector<std::unique_ptr<TPostEffect>>& effects() const { return m_effects; }
        [[nodiscard]] std::vector<std::unique_ptr<TPostEffect>>&       effects() { return m_effects; }

        void onResize( const TPostContext& p_ctx );
        void execute( VkCommandBuffer p_cmd, TPostContext& p_ctx );
        void reloadShaders( const TPostContext& p_ctx );

    private:
        std::vector<std::unique_ptr<TPostEffect>> m_effects;
    };
}  // namespace Tomos
