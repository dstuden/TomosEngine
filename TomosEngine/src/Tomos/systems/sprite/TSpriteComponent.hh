#pragma once

#include <glm/glm.hpp>
#include <string>

#include "Tomos/gpu/TBillboardMode.hh"
#include "Tomos/systems/TComponent.hh"
#include "Tomos/systems/asset/TAssetHandles.hh"

namespace Tomos
{
    class TVkImage;

    // Texture from TSceneResourceBag (see TAssetHandles.hh).
    class TSpriteComponent : public TComponent
    {
    public:
        explicit TSpriteComponent( const TVkImage* p_texture = nullptr ) : m_texture( p_texture ) {}

        TBagTextureRef m_textureRef{};
        const TVkImage*  m_texture = nullptr;

        glm::vec2      m_size     = { 1.0f, 1.0f };
        glm::vec4      m_color    = { 1.0f, 1.0f, 1.0f, 1.0f };
        glm::vec2      m_uvMin    = { 0.0f, 0.0f };
        glm::vec2      m_uvMax    = { 1.0f, 1.0f };
        float          m_rotation = 0.0f;
        TBillboardMode m_mode     = TBillboardMode::Spherical;
        bool           m_visible  = true;
    };
}  // namespace Tomos
