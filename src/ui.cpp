#include <ui.hpp>
#include <imgui.h>
#include <imgui_impl_sdl.h>
#include "imgui_impl_sdlrenderer.h"
#include <SDL2/SDL_opengl.h>


int test;

void init_imgui(SDL_Window* window, SDL_Renderer* renderer)
{

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();

    // Setup Platform/Renderer bindings
    // window is the SDL_Window*
    // context is the SDL_GLContext
    ImGui_ImplSDL2_InitForSDLRenderer(window, renderer);
    ImGui_ImplSDLRenderer_Init(renderer);
}

void update_imgui(SDL_Renderer* renderer)
{   
    // Start the Dear ImGui frame
    ImGui_ImplSDLRenderer_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();

    //imgui window flags
    ImGuiWindowFlags flags = 0;
    flags |= ImGuiWindowFlags_NoTitleBar;
    flags |= !ImGuiWindowFlags_MenuBar;
    flags |= ImGuiWindowFlags_NoResize;

    //render definition Here
    //ImGui::ShowDemoWindow();
    ImGui::Begin("Window",(bool *)true,flags);

    ImGui::NewLine();
    ImGui::NewLine();
    ImGui::SameLine((ImGui::GetWindowWidth()*15)/100);
    ImGui::SliderInt("time",&test,10,100);
    ImGui::NewLine();

    ImGui::SameLine((ImGui::GetWindowWidth()*15)/100);
    ImGui::Button("sound",{35,35});

    ImGui::SameLine((ImGui::GetWindowWidth()*40)/100);
    ImGui::Button("<",{35,35});

    ImGui::SameLine((ImGui::GetWindowWidth()*45)/100);
    if(ImGui::Button("play",{35,35}))
    {
        //toggle_pause(state);  gives error(compiler problem)
    }

    ImGui::SameLine((ImGui::GetWindowWidth()*50)/100);
    if(ImGui::Button(">",{35,35}))
    {

    }

    ImGui::End();
    //imgui Rendering stuff
    ImGui::Render();
    ImGui_ImplSDLRenderer_RenderDrawData(ImGui::GetDrawData());
    SDL_RenderPresent(renderer);
}

void imgui_event_handler(SDL_Event& event){
    ImGui_ImplSDL2_ProcessEvent(&event);
}

void destroy_imgui_data()
{
    // Cleanup
    ImGui_ImplSDLRenderer_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
}

bool want_capture_mouse()
{
    return ImGui::GetIO().WantCaptureMouse;
}

bool want_capture_keyboard()
{
    return ImGui::GetIO().WantCaptureKeyboard;
}