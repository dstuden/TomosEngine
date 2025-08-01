//
// Created by dstuden on 5/9/25.
//

#include "ImGuiLayer.hh"

#include "Tomos/core/TApplication.hh"
#include "Tomos/lib/imgui/imgui.h"
#include "Tomos/lib/imgui/imgui_impl_glfw.h"
#include "Tomos/lib/imgui/imgui_impl_opengl3.h"
#include "Tomos/lib/imgui/imgui_internal.h"
#include "Tomos/util/logger/TLogger.hh"

namespace Tomos
{
    ImGuiLayer::ImGuiLayer( const std::string& p_name ) : TLayer( p_name ) {}

    ImGuiLayer::~ImGuiLayer() {}

    void ImGuiLayer::onUpdate()
    {
        ImGuiIO&      io  = ImGui::GetIO();
        TApplication* app = TApplication::get();
        io.DisplaySize    = ImVec2( app->getWindow().getData().m_width, app->getWindow().getData().m_height );

        float time   = ( float ) glfwGetTime();
        io.DeltaTime = m_time > 0.0f ? ( time - m_time ) : ( 1.0f / 60.0f );
        m_time       = time;

        ImGui_ImplOpenGL3_NewFrame();
        ImGui::NewFrame();

        static bool show = true;
        ImGui::ShowDemoWindow( &show );

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData( ImGui::GetDrawData() );
    }

    void ImGuiLayer::onEvent( TEvent& p_event )
    {
        // Handle events here
    }

    void ImGuiLayer::onAttach()
    {
        // Create ImGui context.
        ImGui::CreateContext();

        // Configure ImGui style.
        ImGui::StyleColorsDark();

        // Get ImGui IO object.
        ImGuiIO& io = ImGui::GetIO();
        io.BackendFlags |= ImGuiBackendFlags_HasMouseCursors;
        io.BackendFlags |= ImGuiBackendFlags_HasSetMousePos;

        // CRITICAL FIX: Initialize ImGui's GLFW backend.
        // This links ImGui to your GLFW window and enables input handling.
        // We get the GLFWwindow handle from the TApplication's window object.
        ImGui_ImplGlfw_InitForOpenGL( TApplication::get()->getWindow().getNativeWindow(), true );

        // Initialize ImGui's OpenGL3 backend.
        ImGui_ImplOpenGL3_Init( "#version 410" );
    }

    void ImGuiLayer::onDetach()
    {
        // Shutdown ImGui backends and destroy context.
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
    }
}  // namespace Tomos
