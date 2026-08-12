#pragma once

#include "Tomos/ui/editor/TSceneEditorContext.hh"

struct ImGuiPayload;

namespace Tomos
{
    class TSceneNode;

    class TAssetBrowserPanel
    {
    public:
        static constexpr const char* k_payloadMesh     = "TOMOS_ASSET_MESH";
        static constexpr const char* k_payloadAudio    = "TOMOS_ASSET_AUDIO";
        static constexpr const char* k_payloadTexture  = "TOMOS_ASSET_TEXTURE";
        static constexpr const char* k_payloadGpuAsset = "TOMOS_GPU_ASSET";

        static void draw( TSceneEditorContext& p_ctx );

        static bool applyDrop( TSceneEditorContext& p_ctx, TSceneNode& p_node, const ImGuiPayload& p_payload );
    };
}  // namespace Tomos
