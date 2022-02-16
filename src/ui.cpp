#include <ui.hpp>
#include <imgui.h>
#include <imgui_impl_sdl.h>
#include "imgui_impl_sdlrenderer.h"
#include <SDL2/SDL_opengl.h>


int test;
int soundvar;
bool stay = false;

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

    //imgui child window flags
    ImGuiWindowFlags child_window_flags = 0;
    child_window_flags |= ImGuiWindowFlags_NoTitleBar;
    child_window_flags |= !ImGuiWindowFlags_MenuBar;
    child_window_flags |= ImGuiWindowFlags_NoResize;
    child_window_flags |= ImGuiWindowFlags_NoMove;


    //imgui style flags
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 12;
    style.ChildRounding = 12;
    style.TabRounding = 12;
    style.FrameRounding = 12;
    style.GrabRounding = 12;

    ImVec2 win_pos;
    //render definition Here
    //ImGui::ShowDemoWindow(); //uncomment to show imgui demo window
    ImGui::Begin("Window",(bool *)true,flags);
    win_pos = ImGui::GetWindowPos();
    win_pos.x += 120;
    win_pos.y -= 110;
    ImGui::NewLine();
    ImGui::NewLine();
    ImGui::SameLine((ImGui::GetWindowWidth()*10)/100);
    ImGui::PushItemWidth((ImGui::GetWindowWidth()*80)/100);
    ImGui::SliderInt(" ",&test,10,100);
    ImGui::NewLine();

    ImGui::SameLine((ImGui::GetWindowWidth()*15)/100);
    if(ImGui::Button("S",{35,35}))
    {
        stay = !stay;
    }
    if(stay)
    {
        ImGui::Begin("sound_window",nullptr,child_window_flags);
        ImGui::SetWindowPos(win_pos);
        ImGui::PushItemWidth((ImGui::GetWindowWidth()*90)/100);
        ImGui::VSliderInt("##int",{20,80},&soundvar,0,100);
        ImGui::End();
    }

    ImGui::SameLine((ImGui::GetWindowWidth()*42)/100);
    if(ImGui::Button("<",{35,35}))
    {

    }

    ImGui::SameLine((ImGui::GetWindowWidth()*47.5)/100);
    if(ImGui::Button("P",{35,35}))
    {
        //toggle_pause(state);  gives error(compiler problem)
    }

    ImGui::SameLine((ImGui::GetWindowWidth()*53)/100);
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