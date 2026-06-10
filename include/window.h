
#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_opengl3.h"
#include <stdio.h>
#include <SDL3/SDL.h>
#include "emulator.h"
#include <SDL3/SDL_opengl.h>
#include <string>

static bool done = false;
static bool errored = false;
static SDL_GLContext gl_context;
static SDL_GLContext debugGlContext;
static Emulator *emu = new Emulator();
static ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

class Window {
private:
    ImGuiIO mainIo;
    ImGuiIO debugIo;
    SDL_Window *_mainWindow;
    SDL_Window *_debugWindow;
    bool drawErrorWin;
    bool drawDbg;
    string errMsg;
public:
    Window();
    ~Window();
    void Run();
    void Render();
    SDL_Window *GetWindow() { return _mainWindow; }
    SDL_Window *GetDebugWindow() { return _debugWindow; }
    void ShowErrorWindow() { drawErrorWin = true; }
    void SetErrorMessage(string msg) { errMsg = msg; }
    void CheckInput(SDL_Window *window, SDL_Event event) {
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL3_ProcessEvent(&event);
            if (event.type == SDL_EVENT_QUIT)
                done = true;
            if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED && event.window.windowID == SDL_GetWindowID(window))
                done = true;
            if (event.type == SDL_EVENT_KEY_DOWN) {
                if (event.key.key == SDLK_BACKSPACE)
                    done = true;
                if ((event.key.mod | SDL_KMOD_CTRL || event.key.mod | SDL_KMOD_GUI) && event.key.key == SDLK_O) {
                    emu->promptForFile(_mainWindow);
                }
                if (event.key.key == SDLK_P)
                    emu->SetStep(true); //we can step now
            }
        }
    }
};
