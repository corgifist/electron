#include "editor_core.h"
#include "ui_api.h"

using namespace Electron;

extern "C" {
    ELECTRON_EXPORT void ProjectConfigurationRender(AppInstance* instance) {
        static bool dockInitialized = false;

        if (instance->isNativeWindow) {
            UIDockSpaceOverViewport(UIGetViewport(), ImGuiDockNodeFlags_PassthruCentralNode, nullptr);
        }

        ImGuiWindowFlags dockFlags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_MenuBar;
        if (instance->isNativeWindow) {
            dockFlags |= ImGuiWindowFlags_NoTitleBar;

            ImVec2 displaySize = UIGetDisplaySize();
            UISetNextWindowPos({0, 0}, ImGuiCond_Once);
            UISetNextWindowSize({640, 480}, ImGuiCond_Once);
        } else {
            UISetNextWindowSize({640, 480}, ImGuiCond_Once);
        }
 
        UIBegin(CSTR(ElectronImplTag(ELECTRON_GET_LOCALIZATION(instance, "PROJECT_CONFIGURATION_WINDOW_TITLE"), instance)), instance->isNativeWindow ? ElectronSignal_None : ElectronSignal_CloseEditor, dockFlags);
            std::string projectTip = ELECTRON_GET_LOCALIZATION(instance, "PROJECT_CONFIGURATION_CREATE_PROJECT_TIP");
            ImVec2 windowSize = UIGetWindowSize();
            ImVec2 tipSize = UICalcTextSize(projectTip.c_str());
    

            if (UIBeginMenuBar()) {
                if (UIBeginMenu(ELECTRON_GET_LOCALIZATION(instance, "PROJECT_CONFIGURATION_MENU_BAR_PROJECT_MENU"))) {
                    if (UIMenuItem(ELECTRON_GET_LOCALIZATION(instance, "PROJECT_CONFIGURATION_MENU_BAR_PROJECT_MENU_OPEN"), "Ctrl+P+O")) {
                        FileDialogImplOpenDialog("OpenProjectDialog", "Open project", nullptr, ".");
                    }
                    if (UIMenuItemEnhanced(CSTR(std::string(ELECTRON_GET_LOCALIZATION(instance, "PROJECT_CONFIGURATION_OPEN_RECENT_PROJECT")) + ": " + JSON_AS_TYPE(instance->configMap["LastProject"], std::string)), "Ctrl+P+L", JSON_AS_TYPE(instance->configMap["LastProject"], std::string) != "null")) {
                        ProjectMap project{};
                        project.path = instance->configMap["LastProject"];
                        project.propertiesMap = json_t::parse(std::fstream(std::string(project.path) + "/project.json"));
                    }
                    UISeparator();
                    if (UIMenuItem(ELECTRON_GET_LOCALIZATION(instance, "RELOAD_APPLICATION"), "")) {
                        UIEndMenu();
                        UIEndMenuBar();
                        UIEnd();
                        throw ElectronSignal_ReloadSystem;
                    }
                    if (UIMenuItem(ELECTRON_GET_LOCALIZATION(instance, "PROJECT_CONFIGURAITON_MENU_BAR_PROJECT_MENU_EXIT"), "Ctrl+P+E")) {
                        ShortcutsImplCtrlPE(instance);
                    }
                    UIEndMenu();
                }
                if (UIBeginMenu(ELECTRON_GET_LOCALIZATION(instance, "PROJECT_CONFIGURATION_MENU_BAR_WINDOW_MENU"))) {
                    if (UIMenuItem(ELECTRON_GET_LOCALIZATION(instance, "PROJECT_CONFIGURATION_MENU_BAR_WINDOW_MENU_RENDER_PREVIEW"), "Ctrl+W+R")) {
                        ShortcutsImplCtrlWR(instance);
                    }
                    if (UIMenuItem(ELECTRON_GET_LOCALIZATION(instance, "LAYER_PROPERTIES_TITLE"), "Ctrl+W+L")) {
                        ShortcutsImplCtrlWL(instance);
                    }
                    UIEndMenu();
                }
                UIEndMenuBar();
            }

        if (UIBeginTabBar(ELECTRON_GET_LOCALIZATION(instance, "PROJECT_CONFIGURATION_CONFIGURATIONS"), 0)) {
            if (UIBeginTabItem(ELECTRON_GET_LOCALIZATION(instance, "PROJECT_CONFIGURATION_PROJECT_CONFIGURATION"), nullptr, 0)) {
            if (instance->projectOpened) {
                ProjectMap& project = instance->project;
                std::string projectConfigurationTitle = ELECTRON_GET_LOCALIZATION(instance, "PROJECT_CONFIGURATION_PROJECT_CONFIGURATION");

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

                int projectFramerate = JSON_AS_TYPE(project.propertiesMap["Framerate"], int);
                float fProjectFramerate = (float) projectFramerate;
                UISliderFloat(ELECTRON_GET_LOCALIZATION(instance, "PROJECT_CONFIGURATION_FRAMERATE"), &fProjectFramerate, 1, 60, "%0.0f", 0);
                projectFramerate = (int) fProjectFramerate;
                project.propertiesMap["Framerate"] = projectFramerate;
            }

            if (!instance->projectOpened) {
                UISetCursorPos(ImVec2{
                    windowSize.x / 2.0f - tipSize.x / 2.0f,
                    windowSize.y / 2.0f - tipSize.y / 2.0f
                });
                UIText(projectTip.c_str());
            }
            UIEndTabItem();
            }
            if (UIBeginTabItem(ELECTRON_GET_LOCALIZATION(instance, "PROJECT_CONFIGURATION_EDITOR_CONFIGURATION"), nullptr, 0)) {
                UIPushFont(instance->largeFont);
                    std::string editorConfigurationString = ELECTRON_GET_LOCALIZATION(instance, "PROJECT_CONFIGURATION_EDITOR_CONFIGURATION");
                    ImVec2 editorConfigurationTextSize = UICalcTextSize(CSTR(editorConfigurationString));
                    UISetCursorX(windowSize.x / 2.0f - editorConfigurationTextSize.x / 2.0f);
                    UIText(CSTR(editorConfigurationString));
                UIPopFont();
                UISeparator();
                
                bool usingNativeWindow = JSON_AS_TYPE(instance->configMap["ViewportMethod"], std::string) == "native-window";
                static std::string viewportMethods[] = {
                    ELECTRON_GET_LOCALIZATION(instance, "PROJECT_CONFIGURATION_EDITOR_VIEWPORT"),
                    ELECTRON_GET_LOCALIZATION(instance, "PROJECT_CONFIGURATION_EDITOR_NATIVE_WINDOW")
                };
                static int selectedViewportMethod = usingNativeWindow ? 1 : 0;

                if (UIBeginCombo(ELECTRON_GET_LOCALIZATION(instance, "PROJECT_CONFIGURATION_EDITOR_CONFIGURATION_VIEWPORT_METHOD"), CSTR(viewportMethods[selectedViewportMethod]))) {
                    for (int i = 0; i < 2; i++) {
                        bool methodSelected = (i == selectedViewportMethod);
                        if (UISelectable(CSTR(viewportMethods[i]), methodSelected)) {
                            selectedViewportMethod = i;
                        }
                        if (methodSelected) UISetItemFocusDefault();
                    }
                    UIEndCombo();
                }
                if (UIIsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled) && UIBeginTooltip()) {
                    UIText(CSTR(std::string("* ") + ELECTRON_GET_LOCALIZATION(instance, "PROJECT_CONFIGURATION_RESTART_REQUIRED")));
                    if (selectedViewportMethod == 0) 
                        UIText(CSTR("* " + std::string(ELECTRON_GET_LOCALIZATION(instance, "PROJECT_CONFIGURATION_SYSTEMWIDE_PERFORMANCE_DROP"))));
                    UIEndTooltip();
                }
                instance->configMap["ViewportMethod"] = selectedViewportMethod == 0 ? "viewport" : "native-window";

                bool usingLinearFiltering = JSON_AS_TYPE(instance->configMap["TextureFiltering"], std::string) == "linear";
                static std::string textureFilters[] = {
                    ELECTRON_GET_LOCALIZATION(instance, "PROJECT_CONFIGURATION_LINEAR"),
                    ELECTRON_GET_LOCALIZATION(instance, "PROJECT_CONFIGURATION_NEAREST")
                };
                static int selectedTextureFiltering = usingLinearFiltering ? 0 : 1;
                if (UIBeginCombo(ELECTRON_GET_LOCALIZATION(instance, "PROJECT_CONFIGURATION_TEXTURE_FILTERING"), CSTR(textureFilters[selectedTextureFiltering]))) {
                    for (int i = 0; i < 2; i++) {
                        bool filterSelected = (i == selectedTextureFiltering);
                        if (UISelectable(CSTR(textureFilters[i]), filterSelected)) {
                            selectedTextureFiltering = i;
                        }
                        if (filterSelected) UISetItemFocusDefault();
                    }
                    UIEndCombo();
                }

                instance->configMap["TextureFiltering"] = selectedTextureFiltering == 0 ? "linear" : "nearest";
                
                int timelinePrescision = JSON_AS_TYPE(instance->configMap["RenderPreviewTimelinePrescision"], int);
                timelinePrescision = std::clamp(timelinePrescision, 0, 1000);
                UIInputInt(ELECTRON_GET_LOCALIZATION(instance, "PROJECT_CONFIGURATION_RENDER_PREVIEW_TIMELINE_PRESCISION"), &timelinePrescision, 1, 100, 0);
                instance->configMap["RenderPreviewTimelinePrescision"] = timelinePrescision;

                bool resizeInterpolation = JSON_AS_TYPE(instance->configMap["ResizeInterpolation"], bool);
                UICheckbox(ELECTRON_GET_LOCALIZATION(instance, "PROJECT_CONFIGURATION_RESIZE_INTERPOLATION"), &resizeInterpolation);
                instance->configMap["ResizeInterpolation"] = resizeInterpolation;
                UISetItemTooltip(ELECTRON_GET_LOCALIZATION(instance, "PROJECT_CONFIGURATION_SMOOTHNESS_DECREASE"));

                float uiScaling = JSON_AS_TYPE(instance->configMap["UIScaling"], float);
                UISliderFloat(ELECTRON_GET_LOCALIZATION(instance, "PROJECT_CONFIGURATION_EDITOR_UI_SCALING"), &uiScaling, 0.5f, 2.5f, "%0.1f", 0);
                UISetItemTooltip(ELECTRON_GET_LOCALIZATION(instance, "PROJECT_CONFIGURATION_RESTART_REQUIRED"));
                instance->configMap["UIScaling"] = uiScaling;

            UIEndTabItem();
            }
            UIEndTabBar();
        }
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