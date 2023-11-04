#pragma once
#define IMGUI_DEFINE_MATH_OPERATORS
#include "electron.h"
#include "electron_font.h"

#include "ImGui/imgui.h"
#define IMGUI_IMPL_OPENGL_ES3
#include "ImGui/imgui_impl_glfw.h"
#include "ImGui/imgui_impl_opengl3.h"
#include "ImGui/im_scoped.h"
#include "ImGui/imgui_internal.h"

#include "editor_core.h"
#include "graphics_core.h"

#include "shortcuts.h"
#include "servers.h"


namespace UI {
    void DropShadow();
    void Begin(const char* name, Electron::ElectronSignal signal, ImGuiWindowFlags flags);
    void End();
};
namespace Electron {

    class ElectronUI;
    class AppInstance;
    class ProjectConfiguration;
    class RenderPreview;
    class Shortcuts;

    struct ProjectMap {
        json_t propertiesMap;
        std::string path;

        ProjectMap() {};

        void SaveProject();
    };

    class AppInstance {
    public:
        GLFWwindow* displayHandle; // GLFW window handle
        std::vector<ElectronUI*> content; // Array of windows
        json_t localizationMap, configMap; // Localization data and editor config
        GraphicsCore graphics; // Graphics core of Electron
        ProjectMap project; // Project instance
        bool projectOpened; // True if any project is opened
        bool isNativeWindow; // Always true (if config.json is not manually edited)
        bool showBadConfigMessage;
        Shortcuts shortcuts; // Editor shortcuts implementation
        int selectedRenderLayer; // Used in LayerProperties
        AssetRegistry assets; // Library of assets
        float fontSize; // Used in FontAtlas manipulations
        static GLuint shadowTex; // Shadow texture for window dropshadows
        ImGuiContext* context; // Store main ImGui context for using in impl files
        std::string renderer, vendor, version; // GL Constants
        bool ffmpegAvailable; // is FFMpeg available

        ImFont* largeFont;
        
        AppInstance();
        ~AppInstance();

        void Run();
        void Terminate();

        void ExecuteSignal(ElectronSignal signal, int windowIndex, int& destroyWindowTarget, bool& exitEditor);
        void ExecuteSignal(ElectronSignal signal);

        void RestoreBadConfig();

        void RenderCriticalError(std::string text, bool* p_open);

        Electron::ElectronVector2f GetNativeWindowSize();
        Electron::ElectronVector2f GetNativeWindowPos();

        void SetupImGuiStyle();

        int GetCacheIndex();

        void AddUIContent(ElectronUI* ui) {
            this->content.push_back(ui);
        }

        bool ButtonCenteredOnLine(const char* label, float alignment = 0.5f);
    };
}
