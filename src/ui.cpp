#include "ui.hpp"

int counter=0;

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
    //render definition Here
    ImGui::ShowDemoWindow();
    ImGui::Begin("Window");
    if (ImGui::Button("Button"))                            // Buttons return true when clicked (most widgets return true when edited/activated)
        counter++;
    ImGui::SameLine();
    ImGui::Text("counter = %d", counter);

    ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
    ImGui::End();
    //imgui Rendering stuff
    ImGui::Render();
    ImGui_ImplSDLRenderer_RenderDrawData(ImGui::GetDrawData());
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