#pragma once

#include <memory>
#include <string>

#include "TScene.hh"
#include "Tomos/events/TEvent.hh"
#include "Tomos/util/renderer/TFrameBuffer.hh"
#include "Tomos/util/renderer/TShader.hh"
#include "Tomos/util/renderer/TVertexArray.hh"
#include "Tomos/util/renderer/passes/TRenderPass.hh"

namespace Tomos
{
    class TLayer
    {
    public:
        explicit TLayer( const std::string& p_id );

        virtual ~TLayer() = default;

        virtual void onAttach() {}

        virtual void onDetach() {}

        virtual void onUpdate()
        {
            if ( m_sceneManager.activeScene() )
            {
                m_sceneManager.activeScene()->update();
            }
        }

        virtual void onEvent( TEvent& p_event ) {}

        void onResize( int p_width, int p_height );

        inline TSceneManager& getSceneManager() { return m_sceneManager; }

        const std::string& getLayerId() const { return m_layerId; }

        void                                             addRenderPass( std::shared_ptr<TRenderPass> p_pass );
        const std::vector<std::shared_ptr<TRenderPass>>& getRenderPasses() const;

        std::shared_ptr<LayerFrameBuffer> getLayerFramebuffer() const { return m_layerFramebuffer; }

        inline const std::shared_ptr<TShader>& getShader() const { return m_shader; }
        void                                   setShader( const std::shared_ptr<TShader>& p_shader ) { m_shader = p_shader; }

        inline const std::shared_ptr<TVertexArray>& getQuad() const { return m_quad; }
        void                                        setQuad( const std::shared_ptr<TVertexArray>& p_quad ) { m_quad = p_quad; }

        void setActive( bool p_active ) { m_active = p_active; }
        bool isActive() const { return m_active; }

    protected:
        std::string m_layerId;

        TSceneManager m_sceneManager;

        std::vector<std::shared_ptr<TRenderPass>> m_passes;
        std::shared_ptr<LayerFrameBuffer>         m_layerFramebuffer;
        std::shared_ptr<TVertexArray>             m_quad;
        std::shared_ptr<TShader>                  m_shader;

        bool m_active = true;
        ;
    };

    /**
     * Not a real stack but it behaves like one for event propagation and layer management.
     */
    class LayerStack
    {
    public:
        LayerStack();
        ~LayerStack();

        // Push a layer and an overlay
        void pushLayer( std::shared_ptr<TLayer> p_layer );
        void pushOverlay( std::shared_ptr<TLayer> p_overlay );

        // Pop a layer or an overlay by its shared_ptr
        void popLayer( const std::shared_ptr<TLayer>& p_layer );
        void popOverlay( const std::shared_ptr<TLayer>& p_overlay );

        // Pop a layer by its ID (convenience function)
        void popLayerById( const std::string& p_layerId );

        // Fast access to a layer by its ID
        std::shared_ptr<TLayer>       getLayer( const std::string& p_layerId );
        const std::shared_ptr<TLayer> getLayer( const std::string& p_layerId ) const;

        std::set<std::string> getLayerIds() const;

        // Iterators for ordered traversal
        std::vector<std::shared_ptr<TLayer>>::iterator         begin() { return m_layers.begin(); }
        std::vector<std::shared_ptr<TLayer>>::iterator         end() { return m_layers.end(); }
        std::vector<std::shared_ptr<TLayer>>::reverse_iterator rbegin() { return m_layers.rbegin(); }
        std::vector<std::shared_ptr<TLayer>>::reverse_iterator rend() { return m_layers.rend(); }

        std::vector<std::shared_ptr<TLayer>>::const_iterator         begin() const { return m_layers.begin(); }
        std::vector<std::shared_ptr<TLayer>>::const_iterator         end() const { return m_layers.end(); }
        std::vector<std::shared_ptr<TLayer>>::const_reverse_iterator rbegin() const { return m_layers.rbegin(); }
        std::vector<std::shared_ptr<TLayer>>::const_reverse_iterator rend() const { return m_layers.rend(); }

        void clear()
        {
            m_layers.clear();
            m_layerMap.clear();
            m_layerInsertIndex = 0;
        }

    private:
        std::vector<std::shared_ptr<TLayer>>                     m_layers;
        std::unordered_map<std::string, std::shared_ptr<TLayer>> m_layerMap;

        unsigned int m_layerInsertIndex = 0;
    };
}  // namespace Tomos
