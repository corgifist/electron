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

#ifdef ELECTRON_IMPLEMENTATION_MODE
    #define DIRECT_SIGNAL(instance, signal) ElectronImplDirectSignal(instance, signal)
#else
    #define DIRECT_SIGNAL(instance, signal) instance->ExecuteSignal(signal)
#endif


namespace UI {
    void Begin(const char* name, Electron::ElectronSignal signal, ImGuiWindowFlags flags);
    void End();
};
namespace Electron {

    class ElectronUI;
    class AppInstance;
    class ProjectConfiguration;
    class RenderPreview;
    class Shortcuts;

    static ImVec4 color_from_hex(std::string l) {
        std::string rgbcolor = l;

        std::regex pattern("#?([0-9a-fA-F]{2})([0-9a-fA-F]{2})([0-9a-fA-F]{2})");

        std::smatch match;
        if (std::regex_match(rgbcolor, match, pattern)) {
            auto r = std::stoul(match[1].str(), nullptr, 16);
            auto g = std::stoul(match[2].str(), nullptr, 16);
            auto b = std::stoul(match[3].str(), nullptr, 16);
            return ImVec4((float) r / 255.0f, (float) g / 255.0f, (float) b / 255.0f, 1.0f);
        } else {
            std::cout << rgbcolor << " is an invalid rgb color\n";
        }
        throw std::runtime_error("malformed hex color " + l);
    }

    struct ProjectMap {
        json_t propertiesMap;
        std::string path;

        ProjectMap() {};

        void SaveProject();
    };

    typedef void (*Electron_ShortcutF)();
    struct ShortcutPair {
        std::vector<ImGuiKey> keys;
        Electron_ShortcutF impl;

        ShortcutPair() {}
    };

    class AppInstance {
    public:
        GLFWwindow* displayHandle;
        std::vector<ElectronUI*> content;
        json_t localizationMap, configMap;
        GraphicsCore graphics;
        ProjectMap project;
        bool projectOpened;
        bool isNativeWindow;
        bool showBadConfigMessage;
        Shortcuts shortcuts;
        ImGuiID nativeWindowCentralDockID;
        int selectedRenderLayer;
        AssetRegistry assets;
        float fontSize;
        std::vector<ShortcutPair> shortcutsPair;
        static GLuint shadowTex;
        ImGuiContext* context;

        ImFont* largeFont;
        
        AppInstance();
        ~AppInstance();

        void Run();
        void Terminate();

        void ExecuteSignal(ElectronSignal signal, int windowIndex, int& destroyWindowTarget, bool& exitEditor);
        void ExecuteSignal(ElectronSignal signal);

        void RestoreBadConfig();

        Electron::ElectronVector2f GetNativeWindowSize();
        Electron::ElectronVector2f GetNativeWindowPos();

        void SetupImGuiStyle();

        void AddUIContent(ElectronUI* ui) {
            this->content.push_back(ui);
        }
        
        void AddShortcut(std::vector<ImGuiKey> keys, Electron_ShortcutF impl) {
            ShortcutPair pair{};
            pair.keys = keys;
            pair.impl = impl;
            shortcutsPair.push_back(pair);
        }

        bool ButtonCenteredOnLine(const char* label, float alignment = 0.5f);
    };
}
