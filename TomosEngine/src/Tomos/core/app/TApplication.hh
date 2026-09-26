#pragma once

#include <memory>
#include <string>

#include "Tomos/core/layers/TLayerStack.hh"
#include "Tomos/core/scene/TSceneManager.hh"
#include "Tomos/core/window/TWindow.hh"
#include "Tomos/systems/asset/TAssetLoadQueue.hh"
#include "Tomos/systems/asset/TAssetSystem.hh"
#include "Tomos/util/config/TConfig.hh"
#include "Tomos/util/memory/TFrameAllocator.hh"
#include "Tomos/util/time/TTime.hh"

namespace Tomos
{
    class TVkGpu;
    class TWindowCloseEvent;
    class TWindowResizeEvent;

    class TApplication
    {
    public:
        // p_props seed the window only when no config file exists yet.
        // Empty p_configPath honors a prior TPath::setConfigPath(); otherwise defaults to "tomos.json".
        explicit TApplication( const TWindowProps& p_props = TWindowProps{}, const std::string& p_configPath = {} );

        virtual ~TApplication();

        TApplication( const TApplication& )            = delete;
        TApplication& operator=( const TApplication& ) = delete;

        void run();

        void close() { m_running = false; }

        [[nodiscard]] bool isRunning() const { return m_running; }

        // Must be called after the window exists, before any TSceneLayer.
        void initGpu( bool p_validation = true );

        void pushLayer( std::unique_ptr<TLayer> p_layer );
        void pushOverlay( std::unique_ptr<TLayer> p_overlay );

        [[nodiscard]] TWindow&          window() { return *m_window; }
        [[nodiscard]] TLayerStack&      layerStack() { return m_layerStack; }
        [[nodiscard]] TAssetSystem&     assetSystem() { return m_assetSystem; }
        [[nodiscard]] TAssetLoadQueue&  assetLoadQueue() { return m_assetLoadQueue; }
        [[nodiscard]] TSceneManager&    sceneManager() { return m_sceneManager; }
        [[nodiscard]] TConfigManager&   configManager() { return m_configManager; }
        [[nodiscard]] TEngineConfig&    config() { return m_configManager.get<TEngineConfig>(); }
        [[nodiscard]] TTime&            time() { return m_time; }
        [[nodiscard]] const TTime&      time() const { return m_time; }
        [[nodiscard]] TFrameAllocator&  frameAllocator() { return m_frameAllocator; }

        [[nodiscard]] TVkGpu* gpu() const { return m_gpu.get(); }

        [[nodiscard]] static TApplication& get() { return *g_sInstance; }

    private:
        void onEvent( TEvent& p_event );
        bool onWindowClose( TWindowCloseEvent& p_event );
        bool onWindowResize( TWindowResizeEvent& p_event );

        TConfigManager           m_configManager;
        TSceneManager            m_sceneManager;
        std::unique_ptr<TWindow> m_window;
        std::unique_ptr<TVkGpu>  m_gpu;
        TAssetSystem             m_assetSystem;
        TAssetLoadQueue          m_assetLoadQueue{ m_assetSystem };
        TLayerStack              m_layerStack;
        TTime                    m_time;
        TFrameAllocator          m_frameAllocator;
        bool                     m_running = true;

        static TApplication* g_sInstance;
    };
}  // namespace Tomos
