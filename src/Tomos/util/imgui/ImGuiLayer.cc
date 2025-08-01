//
// Created by dstuden on 5/9/25.
//

#include "ImGuiLayer.hh"

#include "Tomos/core/TApplication.hh"
#include "Tomos/lib/imgui/imgui.h"
#include "Tomos/lib/imgui/imgui_impl_glfw.h"
#include "Tomos/lib/imgui/imgui_impl_opengl3.h"
#include "Tomos/util/logger/TLogger.hh"
#include "Tomos/util/renderer/TBuffer.hh"
#include "Tomos/util/renderer/TRenderer.hh"
#include "Tomos/util/renderer/TVertexArray.hh"

namespace Tomos
{
    ImGuiLayer::ImGuiLayer( const std::string& p_id ) : TLayer( p_id )
    {
        float quadVertices[] = {
                // positions, texcoords
                -1.0f, 1.0f,  0.0f, 1.0f,  // Top-left
                -1.0f, -1.0f, 0.0f, 0.0f,  // Bottom-left
                1.0f,  -1.0f, 1.0f, 0.0f,  // Bottom-right

                -1.0f, 1.0f,  0.0f, 1.0f,  // Top-left
                1.0f,  -1.0f, 1.0f, 0.0f,  // Bottom-right
                1.0f,  1.0f,  1.0f, 1.0f  // Top-right
        };

        auto vbo = std::make_shared<TVertexBuffer>( quadVertices, sizeof( quadVertices ) );
        vbo->setLayout( { { ShaderDataType::Float2, "aPosition" }, { ShaderDataType::Float2, "aTexCoord" } } );

        auto layerQuad = std::make_shared<TVertexArray>();
        layerQuad->addVertexBuffer( vbo );
        this->setQuad( layerQuad );
    }

    ImGuiLayer::~ImGuiLayer() {}

    void ImGuiLayer::onUpdate()
    {
        this->getLayerFramebuffer()->bind();

        TRenderer::setClearedColor( { 0, 0, 0, 0 } );
        TRenderer::clear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        static bool show = true;
        ImGui::ShowDemoWindow( &show );

        ImGui::Render();

        ImGui_ImplOpenGL3_RenderDrawData( ImGui::GetDrawData() );

        this->getLayerFramebuffer()->unbind();
    }

    void ImGuiLayer::onEvent( TEvent& p_event )
    {
        if ( m_blockEvents )
        {
            ImGuiIO& io = ImGui::GetIO();
            p_event.setHandled( p_event.isHandled() | ( p_event.isInCategory( EventCategory::MOUSE ) & io.WantCaptureMouse ) );
            p_event.setHandled( p_event.isHandled() | ( p_event.isInCategory( EventCategory::KEYBOARD ) & io.WantCaptureKeyboard ) );
        }
    }

    void ImGuiLayer::onAttach()
    {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        ( void ) io;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;  // Enable Keyboard Controls

        ImGui::StyleColorsDark();

        ImGui_ImplGlfw_InitForOpenGL( TApplication::get()->getWindow().getNativeWindow(), true );
        ImGui_ImplOpenGL3_Init( "#version 430" );
    }

    void ImGuiLayer::onDetach()
    {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
    }

    void ImGuiLayer::blockEvents( bool p_block ) { m_blockEvents = p_block; }
}  // namespace Tomos
