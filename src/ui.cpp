/*
**  Includes
*/

#include "ui.hpp"

/*
**  Globals
*/

int progressvar;
int sound_var = 128;
int sound_tmp;
bool vol_change = false;
bool stay = false;
static bool first = true;
bool req_pause = false;
bool req_seek = false;
bool req_mute = false;
bool req_trk_chnge = false;
bool draw_ui = true;
double ui_incr;
const char* label_text ="00:00:00";

/*
**  Functions
*/

void init_imgui(SDL_Window* window, SDL_Renderer* renderer)
{

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.IniFilename = NULL;
    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    // Setup Platform/Renderer bindings
    // window is the SDL_Window*
    // context is the SDL_GLContext
    ImGui_ImplSDL2_InitForSDLRenderer(window, renderer);
    ImGui_ImplSDLRenderer_Init(renderer);
}

void update_imgui(SDL_Renderer* renderer, int width, int height)
{   
    if(draw_ui){
        // Start the Dear ImGui frame
        ImGui_ImplSDLRenderer_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        //imgui window flags
        ImGuiWindowFlags flags = 0;
        flags |= ImGuiWindowFlags_NoTitleBar;
        flags |= !ImGuiWindowFlags_MenuBar;
        flags |= ImGuiWindowFlags_NoResize;
        flags |= ImGuiWindowFlags_NoMove;

        //imgui child window flags
        ImGuiWindowFlags child_window_flags = 0;
        child_window_flags |= ImGuiWindowFlags_NoTitleBar;
        child_window_flags |= !ImGuiWindowFlags_MenuBar;
        child_window_flags |= ImGuiWindowFlags_NoResize;
        child_window_flags |= ImGuiWindowFlags_NoMove;


        //imgui style flags
        ImGuiStyle& style = ImGui::GetStyle();
        style.WindowRounding = 5;
        style.ChildRounding = 12;
        style.TabRounding = 12;
        style.FrameRounding = 12;
        style.GrabRounding = 12;

        ImVec2 win_pos;
        ImVec2 main_win_pos;
        ImVec2 win_size;
        ImVec2 window_size;
        //render definition Here
        ImGui::Begin("Window",(bool *)true,flags);
        if (first)
        {
            window_size = { 645 , 61 };
            ImGui::SetWindowSize(window_size);
        }
        win_size = ImGui::GetWindowSize();
        main_win_pos = {(float)(width/2) - (window_size.x/2), (float)height - 60};
        ImGui::SetWindowPos(main_win_pos);
        win_pos = ImGui::GetWindowPos();
        win_pos.y -= 110;
        ImGui::NewLine();
        ImGui::SameLine((ImGui::GetWindowWidth()*1)/100);
        ImGui::LabelText("",label_text);
        ImGui::SameLine((ImGui::GetWindowWidth()*10)/100);
        ImGui::PushItemWidth((ImGui::GetWindowWidth()*80)/100);
        ImGui::SliderInt("00:00:00",&progressvar,10,100);
        ImGui::NewLine();


        ImGui::SameLine((ImGui::GetWindowWidth()*10)/100);
        if(ImGui::Button("M",{(win_size.x*4)/100, 20}))
        {
            req_mute = true;
        }

        ImGui::SameLine((ImGui::GetWindowWidth()*15)/100);
        ImGui::PushItemWidth((ImGui::GetWindowWidth()*20)/100);
        if(ImGui::SliderInt("  ",&sound_var,0,128) || ImGui::IsItemHovered()){
            if(ImGui::GetIO().MouseWheel){
                if((0 <= sound_var) && (sound_var <= 128)){
                    sound_tmp = sound_var + ImGui::GetIO().MouseWheel;
                    if((sound_tmp < 0) || (sound_tmp > 128)){
                        // Do nothing, already at the limits
                    }
                    else{
                        sound_var = sound_tmp;
                    }
                }
            }
            vol_change = true;
        }

        ImGui::SameLine((ImGui::GetWindowWidth()*36)/100);
        if(ImGui::Button("<<",{(win_size.x*5)/100, 20}))
        {
            ui_incr = -60.0;
            req_seek = true;
        }

        ImGui::SameLine((ImGui::GetWindowWidth()*42)/100);
        if(ImGui::Button("<",{(win_size.x*4)/100, 20}))
        {
            ui_incr = -10.0;
            req_seek = true;
        }

        ImGui::SameLine((ImGui::GetWindowWidth()*47.5)/100);
        if(ImGui::Button("P",{(win_size.x*4)/100, 20}))
        {
            req_pause = !req_pause;
        }

        ImGui::SameLine((ImGui::GetWindowWidth()*53)/100);
        if(ImGui::Button(">",{(win_size.x*4)/100, 20}))
        {
            ui_incr = 10.0;
            req_seek = true;
        }

        ImGui::SameLine((ImGui::GetWindowWidth()*58)/100);
        if(ImGui::Button(">>",{(win_size.x*5)/100, 20}))
        {
            ui_incr = 60.0;
            req_seek = true;
        }

        ImGui::SameLine((ImGui::GetWindowWidth()*86)/100);
        if(ImGui::Button("T",{(win_size.x*4)/100, 20}))
        {
            req_trk_chnge = true;
        }

        ImGui::End();
        //imgui Rendering stuff
        ImGui::Render();
        ImGui_ImplSDLRenderer_RenderDrawData(ImGui::GetDrawData());
    }
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

void change_imgui_win_size()
{
    first = true;
}

void end_win_size_change()
{
    first = false;
}