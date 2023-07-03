#include "editor_core.h"
#include "ui_api.h"

using namespace Electron;

extern "C" {
    ELECTRON_EXPORT void ProjectConfigurationRender(AppInstance* instance) {
        static bool dockInitialized = false;

        if (instance->isNativeWindow) {
            UIDockSpaceOverViewport(UIGetViewport(), ImGuiDockNodeFlags_PassthruCentralNode, nullptr);
        }

        UISetNextWindowSize({640, 480}, ImGuiCond_Once);
        ImGuiWindowFlags dockFlags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_MenuBar;
        if (instance->isNativeWindow) {
            dockFlags |= ImGuiWindowFlags_NoTitleBar;
        }

        UIBegin(CSTR(ElectronImplTag(ELECTRON_GET_LOCALIZATION(instance, "PROJECT_CONFIGURATION_WINDOW_TITLE"), instance)), ElectronSignal_CloseEditor, dockFlags);
            std::string projectTip = ELECTRON_GET_LOCALIZATION(instance, "PROJECT_CONFIGURATION_CREATE_PROJECT_TIP");
            ImVec2 windowSize = UIGetWindowSize();
            ImVec2 tipSize = UICalcTextSize(projectTip.c_str());
    

            if (UIBeginMenuBar()) {
                if (UIBeginMenu(ELECTRON_GET_LOCALIZATION(instance, "PROJECT_CONFIGURATION_MENU_BAR_PROJECT_MENU"))) {
                    if (UIMenuItem(ELECTRON_GET_LOCALIZATION(instance, "PROJECT_CONFIGURATION_MENU_BAR_PROJECT_MENU_OPEN"), "Ctrl+P+O")) {
                        FileDialogImplOpenDialog("OpenProjectDialog", "Open project", nullptr, ".");
                    }
                    UISeparator();
                    if (UIMenuItem(ELECTRON_GET_LOCALIZATION(instance, "PROJECT_CONFIGURAITON_MENU_BAR_PROJECT_MENU_EXIT"), "Ctrl+P+E")) {
                        ShortcutsImplCtrlPE(instance);
                    }
                    UIEndMenu();
                }
                if (UIBeginMenu(ELECTRON_GET_LOCALIZATION(instance, "PROJECT_CONFIGURATION_MENU_BAR_WINDOW_MENU"))) {
                    if (UIMenuItem(ELECTRON_GET_LOCALIZATION(instance, "PROJECT_CONFIGURATION_MENU_BAR_WINDOW_MENU_RENDER_PREVIEW"), "Ctrl+W+R")) {
                        ShortcutsImplCtrlWR(instance);
                    }
                    UIEndMenu();
                }
                UIEndMenuBar();
            }

            if (instance->projectOpened) {
                ProjectMap& project = instance->project;
                std::string projectConfigurationTitle = ELECTRON_GET_LOCALIZATION(instance, "PROJECT_CONFIGURATION_WINDOW_TITLE");

                UIPushFont(instance->largeFont);
                    ImVec2 titleSize = UICalcTextSize(CSTR(projectConfigurationTitle));
                    UISetCursorX(windowSize.x / 2.0f - titleSize.x / 2.0f);
                    UIText(CSTR(projectConfigurationTitle));
                UIPopFont();
                UISeparator();

                std::string name = JSON_AS_TYPE(project.propertiesMap["ProjectName"], std::string);
                UIInputField(ELECTRON_GET_LOCALIZATION(instance, "PROJECT_CONFIGURATION_PROJECT_NAME_LABEL"), &name, 0);
                project.propertiesMap["ProjectName"] = name;

                std::vector<int> sourceResolution = JSON_AS_TYPE(project.propertiesMap["ProjectResolution"], std::vector<int>);
                std::vector<int> resolutionPtr = sourceResolution;
                UIInputInt2(ELECTRON_GET_LOCALIZATION(instance, "PROJECT_CONFIGURATION_RESOLUTION_LABEL"), resolutionPtr.data(), 0);
                project.propertiesMap["ProjectResolution"] = resolutionPtr;

                std::vector<float> backgroundColor = JSON_AS_TYPE(project.propertiesMap["BackgroundColor"], std::vector<float>);
                UIInputColor3(ELECTRON_GET_LOCALIZATION(instance, "PROJECT_CONFIGURATION_BACKGROUND_COLOR"), backgroundColor.data(), 0);
                project.propertiesMap["BackgroundColor"] = backgroundColor;

                UIEnd();
                return;
            }

            UISetCursorPos(ImVec2{
                windowSize.x / 2.0f - tipSize.x / 2.0f,
                windowSize.y / 2.0f - tipSize.y / 2.0f
            });
            UIText(projectTip.c_str());
        UIEnd();

        if (FileDialogImplDisplay("OpenProjectDialog")) {
            if (FileDialogImplIsOK()) {
                ProjectMap project{};
                project.path = FileDialogImplGetFilePathName();
                project.propertiesMap = json_t::parse(std::fstream(std::string(project.path) + "/project.json"));
                ShortcutsImplCtrlPO(instance, project);
            }

            FileDialogImplClose();
        }
    }
}