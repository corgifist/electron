#pragma once
#define IMGUI_DEFINE_MATH_OPERATORS
#include "electron.h"
#include "electron_font.h"

#include "ImGui/imgui.h"
#define IMGUI_IMPL_OPENGL_ES3
#include "ImGui/imgui_impl_sdl2.h"
#include "ImGui/imgui_impl_opengl3.h"
#include "ImGui/imgui_internal.h"

#include "editor_core.h"
#include "graphics_core.h"
#include "async_ffmpeg.h"

#include "shortcuts.h"
#include "servers.h"
#include "asset_registry.h"


namespace Electron {

    class ElectronUI;
    class AppInstance;
    class ProjectConfiguration;
    class RenderPreview;
    class Shortcuts;

    namespace UI {
        void Begin(const char* name, Signal signal, ImGuiWindowFlags flags);
        void End();
    };

    // Initializes OpenGL and renders UI
    struct AppInstance {
        SDL_Window* displayHandle; // SDL window handle
        SDL_GLContext glc; // GL Context
        void* rdr; // Renderer
        std::vector<UIActivity> content; // Array of windows
        ImGuiContext* context; // Store main ImGui context for using in impl files
        std::string renderer, vendor, version; // GL Constants
        bool projectOpened; // True if any project is opened
        bool isNativeWindow; // Always true (if config.json is not manually edited)
        bool showBadConfigMessage;
        bool ffmpegAvailable; // is FFMpeg available
        bool running;


        float fontSize; // Used in FontAtlas manipulations
        static ImFont* largeFont;
        static ImFont* mediumFont;
        
        AppInstance();
        ~AppInstance();

        void Run();
        void Terminate();

        void ExecuteSignal(Signal signal, int windowIndex, int& destroyWindowTarget, bool& exitEditor);
        void ExecuteSignal(Signal signal);

        void RestoreBadConfig();

        void RenderCriticalError(std::string text, bool* p_open);

        ImVec2 GetNativeWindowSize();
        ImVec2 GetNativeWindowPos();

        void SetupImGuiStyle();

        int GetCacheIndex();

        void AddUIContent(UIActivity ui) {
            this->content.push_back(ui);
        }

        bool ButtonCenteredOnLine(const char* label, float alignment = 0.5f);
        double GetTime();
    };
}
