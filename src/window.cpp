#include "window.h"

using namespace std;

int multiplier = 4;
int currItem = multiplier - 1;

Window::Window() {
    // GL 3.2 Core + GLSL 150
    const char* glsl_version = "#version 150";
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG); // Always required on Mac
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);

    // Create window with graphics context
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    SDL_WindowFlags window_flags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN | SDL_WINDOW_HIGH_PIXEL_DENSITY;
    _mainWindow = SDL_CreateWindow("SNES Emulator", 160 * multiplier, 144 * multiplier, window_flags);
    if (_mainWindow == nullptr) {
        printf("Error: SDL_CreateWindow(): %s\n", SDL_GetError());
        errored = true;
    }
    gl_context = SDL_GL_CreateContext(_mainWindow);
    if (gl_context == nullptr) {
        printf("Error: SDL_GL_CreateContext(): %s\n", SDL_GetError());
        errored = true;
    }

    _debugWindow = SDL_CreateWindow("Debugger", 200, 100, window_flags);
    if (_debugWindow == nullptr) {
        printf("Error: SDL_CreateWindow(): %s\n", SDL_GetError());
        errored = true;
    }
    debugGlContext = SDL_GL_CreateContext(_debugWindow);
    if (debugGlContext == nullptr) {
        printf("Error: SDL_GL_CreateContext(): %s\n", SDL_GetError());
        errored = true;
    }

    SDL_GL_MakeCurrent(_mainWindow, gl_context);
    SDL_GL_SetSwapInterval(1); // Enable vsync
    SDL_SetWindowPosition(_mainWindow, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    SDL_ShowWindow(_mainWindow);


    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    //Main window
    ImGui::CreateContext();
    mainIo = ImGui::GetIO(); (void)mainIo;
    mainIo.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    mainIo.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    //ImGui::StyleColorsLight();

    // Setup scaling
    ImGuiStyle& style = ImGui::GetStyle();
    style.ScaleAllSizes(1);      
    style.FontScaleDpi = 1;      

    // Setup Platform/Renderer backends
    ImGui_ImplSDL3_InitForOpenGL(_mainWindow, gl_context);
    ImGui_ImplOpenGL3_Init(glsl_version);
    drawErrorWin = false;
    drawDbg = false;
}

void Window::Run() {
    emu->SetWindow(this);
    while (!done) {
        SDL_Event event;
        CheckInput(_mainWindow, event);

        if (SDL_GetWindowFlags(_mainWindow) & SDL_WINDOW_MINIMIZED) {
            SDL_Delay(10);
            continue;
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();

        const ImGuiViewport* vp = ImGui::GetMainViewport();

        //start gui
        if (ImGui::BeginPopupContextVoid("main")) {
            ImGui::Text("Hello");
            if (ImGui::Button("Load ROM")) emu->promptForFile(_mainWindow);
            if (ImGui::Button("Show Debug Window")) drawDbg = true;
            if (ImGui::Button("Exit")) done = true;
            //if (ImGui::Button("States")) 
            ImGui::EndPopup();
        }

        //ImGui::ShowDemoWindow();

        if (drawDbg) {
            ImGui::SetNextWindowPos(ImVec2(vp->Size.x / 2, vp->Size.y / 2), ImGuiCond_FirstUseEver);
            ImGui::SetNextWindowSize(ImVec2(270.0f, 120.0f), ImGuiCond_FirstUseEver);
            if (ImGui::Begin("Debugger", &drawDbg, ImGuiWindowFlags_NoCollapse)) {
                CPU *cpu = emu->GetCPU();
                CPUState state = cpu->GetState();
                ImGui::Text("CPU State:");
                ImGui::Text(
                    "PC: %04X, A: %04X, X: %04X, Y: %04X,\n"
                    "SP: %04X, D: %04x, K: %02X, DBR: %02X", 
                    state.PC, state.A, state.X, state.Y,
                    state.SP, state.D, state.K, state.DBR);
                ImGui::Text("Flags:");
                const char *flagNames [] = {
                    "Carry",
                    "Zero",
                    "Interrupt",
                    "Decimal",
                    "8-Bit Index Mode",
                    "8-Bit Memory Mode",
                    "Overflow",
                    "Negative"
                };
                bool flags [] = {
                    cpu->CheckFlag(ProcessorFlags::Carry), //0
                    cpu->CheckFlag(ProcessorFlags::Zero), //1
                    cpu->CheckFlag(ProcessorFlags::Interrupt), //2
                    cpu->CheckFlag(ProcessorFlags::Decimal), //3
                    cpu->CheckFlag(ProcessorFlags::IndexMode8), //4
                    cpu->CheckFlag(ProcessorFlags::MemoryMode8), //5
                    cpu->CheckFlag(ProcessorFlags::Overflow), //6
                    cpu->CheckFlag(ProcessorFlags::Negative) //7
                };
                for (int i = 0; i < sizeof(flags); i++) {
                    if (ImGui::Checkbox(flagNames[i], &flags[i])) {
                        ImGui::End();
                    }
                }

                ImGui::End();
            }
        }

        if (drawErrorWin) {
            ImGui::SetNextWindowPos(ImVec2(vp->Size.x / 2, vp->Size.y / 2), ImGuiCond_FirstUseEver);
            ImGui::SetNextWindowSize(ImVec2(270.0f, 120.0f), ImGuiCond_FirstUseEver);
            if (ImGui::Begin("Crash!", &drawErrorWin, ImGuiWindowFlags_NoCollapse)) {
                ImGui::Text("%s", errMsg.c_str());
                if (ImGui::Button("Close")) drawErrorWin = false;
                ImGui::End();
            }
        }
        
        Render();
        if (emu->isReady() && !emu->hasStarted()) emu->Start();
    }
}

void Window::Render() {
    //main
    ImGui::Render();
    glViewport(0, 0, (int)mainIo.DisplaySize.x, (int)mainIo.DisplaySize.y);
    glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    SDL_GL_SwapWindow(_mainWindow);

    //debug
    ImGui::Render();
    glViewport(0, 0, (int)debugIo.DisplaySize.x, (int)debugIo.DisplaySize.y);
    glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    SDL_GL_SwapWindow(_debugWindow);
}

Window::~Window() {}