#pragma once

// C++26 reflection annotations (P3394). Requires GCC 16+ with -freflection.
// Annotation types are const-qualified when queried via std::meta::annotations_of —
// helpers strip const.
//
// TOMOS_ANN: expands to [[=...]] only when the compiler implements reflection.
// Clangd / upstream Clang do not — leaving annotations raw causes "Expected ']'".

#if defined( __cpp_impl_reflection ) && __cpp_impl_reflection >= 202400L
#define TOMOS_ANN( ... ) [[=__VA_ARGS__]]
#else
#define TOMOS_ANN( ... )
#endif

namespace Tomos::Reflect
{
    // Omit from JSON and default ImGui field editors.
    struct Skip
    {
    };

    // Serialize to JSON, but skip in default ImGui editors.
    struct UiSkip
    {
    };

    // Override JSON key (default strips leading "m_"). Fixed buffer — string_view
    // pointers inside annotations are not reliably extractable on GCC 16.
    struct JsonKey
    {
        char name[ 32 ]{};
    };

    // ImGui display label for fields (default = JSON key). Enumerator annotations
    // are not supported on GCC 16 yet.
    struct UiLabel
    {
        char name[ 64 ]{};
    };

    // ImGui slider/drag range for float/int fields.
    struct UiRange
    {
        float min = 0.0f;
        float max = 1.0f;
    };

    // Store radians; edit as degrees in ImGui. JSON stays radians.
    struct Degrees
    {
    };

    // Prefer ColorEditN over DragFloatN for glm::vec3/vec4.
    struct UiColor
    {
    };

    // Component registry identity for makePodComponent / makeComponentStub.
    struct ComponentMeta
    {
        char type[ 32 ]{};
        char label[ 64 ]{};
    };
}  // namespace Tomos::Reflect
