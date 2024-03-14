#pragma once
#define IMGUI_DEFINE_MATH_OPERATORS
#include "electron.h"
#include "electron_font.h"

#include "ImGui/imgui.h"
#include "ImGui/imgui_internal.h"

#include "editor_core.h"
#include "graphics_core.h"
#include "async_ffmpeg.h"

#include "shortcuts.h"
#include "servers.h"
#include "asset_core.h"
#include "driver_core.h"
#include "build_number.h"


namespace Electron {
    namespace UI {
        void Begin(const char* name, Signal signal, ImGuiWindowFlags flags);
        void End();
    };

    // Initializes OpenGL and renders UI
    struct AppCore {
        static std::vector<UIActivity> content; // Array of windows
        static ImGuiContext* context; // Store main ImGui context for using in impl files
        static bool projectOpened; // True if any project is opened
        static bool showBadConfigMessage;
        static bool ffmpegAvailable; // is FFMpeg available
        static std::thread* asyncWriter;
        static bool running;


        static float fontSize; // Used in FontAtlas manipulations
        static ImFont* largeFont;
        static ImFont* mediumFont;
        
        static void Initialize();

        static void Run();
        static void Terminate();

        static void ExecuteSignal(Signal signal, int windowIndex, int& destroyWindowTarget, bool& exitEditor);
        static void ExecuteSignal(Signal signal);

        static void RestoreBadConfig();
        static void RenderCriticalError(std::string text, bool* p_open);
        static void PushNotification(int duration, std::string text);

        static ImVec2 GetNativeWindowSize();
        static ImVec2 GetNativeWindowPos();

        static void SetupImGuiStyle();
        static int GetCacheIndex();

        static void AddUIContent(UIActivity ui) {
            AppCore::content.push_back(ui);
        }

        static bool ButtonCenteredOnLine(const char* label, float alignment = 0.5f);
        static double GetTime();
    };
}
