#pragma once

#include "Tomos/ui/editor/TSceneEditorContext.hh"

namespace Tomos
{
    class TRendererDebugPanel
    {
    public:
        static void draw( TSceneEditorContext& p_ctx, float p_dt );
    };
}  // namespace Tomos
