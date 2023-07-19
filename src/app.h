#pragma once
#define IMGUI_DEFINE_MATH_OPERATORS
#include "electron.h"
#include "electron_font.h"

#include "ImGui/imgui.h"
#include "ImGui/imgui_impl_glfw.h"
#include "ImGui/imgui_impl_opengl3.h"
#include "ImGui/im_scoped.h"
#include "ImGui/imgui_internal.h"

#include "editor_core.h"
#include "graphics_core.h"

#include "shortcuts.h"

#define ELECTRON_JSON_TO_STRING(x) 
#define ELECTRON_GET_LOCALIZATION(instance, localization) (((instance->localizationMap[localization]).template get<std::string>()).c_str())

#ifdef ELECTRON_IMPLEMENTATION_MODE
    #define DIRECT_SIGNAL(instance, signal) ElectronImplDirectSignal(instance, signal)
#else
    #define DIRECT_SIGNAL(instance, signal) instance->ExecuteSignal(signal)
#endif

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
    private:
        GLFWwindow* displayHandle;
        std::vector<ElectronUI*> content;
    public:
        json_t localizationMap, configMap;
        GraphicsCore graphics;
        ProjectMap project;
        bool projectOpened;
        bool isNativeWindow;
        bool showBadConfigMessage;
        Shortcuts shortcuts;
        ImGuiID nativeWindowCentralDockID;

        ImFont* largeFont;
        
        AppInstance();

        void Run();

        void ExecuteSignal(ElectronSignal signal, int windowIndex, int& destroyWindowTarget, bool& exitEditor);
        void ExecuteSignal(ElectronSignal signal);

        void RestoreBadConfig();

        Electron::ElectronVector2f GetNativeWindowSize();
        Electron::ElectronVector2f GetNativeWindowPos();

        void AddUIContent(ElectronUI* ui) {
            this->content.push_back(ui);
        }
    private:
        bool ButtonCenteredOnLine(const char* label, float alignment = 0.5f);
    };
}
