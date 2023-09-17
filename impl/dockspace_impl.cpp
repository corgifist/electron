#include "editor_core.h"
#include "ui_api.h"

using namespace Electron;

extern "C" {

    int UIFormatString(char* buf, size_t buf_size, const char* fmt, ...) {
        va_list args;
        va_start(args, fmt);
        int w = vsnprintf(buf, buf_size, fmt, args);
        va_end(args);
        if (buf == NULL)
            return w;
        if (w == -1 || w >= (int)buf_size)
            w = (int)buf_size - 1;
        buf[w] = 0;
        return w;
    }

    UIBeginMenuBar_T MenuBarBeginProc = UIBeginMenuBar;
    UIEndMenuBar_T MenuBarEndProc = UIEndMenuBar;

    void DockspaceRenderTabBar(AppInstance* instance) {
        if (MenuBarBeginProc()) {
                if (UIBeginMenu(string_format("%s %s", ICON_FA_FOLDER_OPEN, ELECTRON_GET_LOCALIZATION(instance, "PROJECT_CONFIGURATION_MENU_BAR_PROJECT_MENU")).c_str())) {
                    if (UIMenuItem(CSTR(ICON_FA_FOLDER_OPEN + std::string(" ") + ELECTRON_GET_LOCALIZATION(instance, "PROJECT_CONFIGURATION_MENU_BAR_PROJECT_MENU_OPEN")), "Ctrl+P+O")) {
                        FileDialogImplOpenDialog("OpenProjectDialog", "Open project", nullptr, ".");
                    }
                    if (UIMenuItemEnhanced(CSTR(ICON_FA_FOLDER_OPEN + std::string(" ") + std::string(ELECTRON_GET_LOCALIZATION(instance, "PROJECT_CONFIGURATION_OPEN_RECENT_PROJECT")) + ": " + JSON_AS_TYPE(instance->configMap["LastProject"], std::string)), "Ctrl+P+L", JSON_AS_TYPE(instance->configMap["LastProject"], std::string) != "null")) {
                        ProjectMap project{};
                        project.path = instance->configMap["LastProject"];
                        project.propertiesMap = json_t::parse(std::fstream(std::string(project.path) + "/project.json"));
                        ShortcutsImplCtrlPO(instance, project);
                    }
                    UISeparator();
                    if (UIMenuItem(CSTR(ICON_FA_RECYCLE + std::string(" ") + std::string(ELECTRON_GET_LOCALIZATION(instance, "RELOAD_APPLICATION"))), "")) {
                        UIEndMenu();
                        UIEndMenuBar();
                        UIEnd();
                        throw ElectronSignal_ReloadSystem;
                    }
                    if (UIMenuItem(CSTR(ICON_FA_CROSS + std::string(" ") + std::string(ELECTRON_GET_LOCALIZATION(instance, "PROJECT_CONFIGURAITON_MENU_BAR_PROJECT_MENU_EXIT"))), "Ctrl+P+E")) {
                        ShortcutsImplCtrlPE(instance);
                    }
                    UIEndMenu();
                }
                if (UIBeginMenu(CSTR(ICON_FA_LAYER_GROUP + std::string(" ") + std::string(ELECTRON_GET_LOCALIZATION(instance, "PROJECT_CONFIGURATION_MENU_BAR_LAYERS"))))) {
                    auto registry = InternalGetDylibRegistry();
                    for (auto& entry : registry) {
                        std::string key = entry.first;

                        if (UIMenuItem(registry.at(key).get_variable<std::string>("LayerName").c_str(), "")) {
                            GraphicsImplAddRenderLayer(&instance->graphics, key);
                        }
                    }
                    UIEndMenu();
                }
                if (UIBeginMenu(CSTR(std::string(ICON_FA_TOOLBOX + std::string(" ") + ELECTRON_GET_LOCALIZATION(instance, "PROJECT_CONFIGURATION_MENU_BAR_WINDOW_MENU"))))) {
                    if (UIMenuItemEnhanced(ELECTRON_GET_LOCALIZATION(instance, "PROJECT_CONFIGURATION_MENU_BAR_WINDOW_MENU_RENDER_PREVIEW"), "Ctrl+W+R", CounterGetRenderPreview() != 1)) {
                        ShortcutsImplCtrlWR(instance);
                    }
                    if (UIMenuItemEnhanced(ELECTRON_GET_LOCALIZATION(instance, "LAYER_PROPERTIES_TITLE"), "Ctrl+W+L", CounterGetLayerProperties() != 1)) {
                        ShortcutsImplCtrlWL(instance);
                    }
                    if (UIMenuItemEnhanced(ELECTRON_GET_LOCALIZATION(instance, "ASSET_MANAGER_TITLE"), "Ctrl+W+A", CounterGetAssetManager() != 1)) {
                        ShortcutsImplCtrlWA(instance);
                    }
                    if (UIMenuItemEnhanced(ELECTRON_GET_LOCALIZATION(instance, "TIMELINE_TITLE"), "Ctrl+W+T", CounterGetTimelineCounter() != 1)) {
                        ShortcutsImplCtrlWT(instance);
                    }
                    UIEndMenu();
                }
                MenuBarEndProc();
            }
    }

    ELECTRON_EXPORT void DockspaceRender(AppInstance* instance) {
          {
            ImGuiViewport* viewport = UIGetViewport();
            ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_PassthruCentralNode;

            UISetNextWindowPos(viewport->WorkPos, 0);
            UISetNextWindowSize(viewport->WorkSize, 0);
            UISetNextWindowViewport(viewport->ID);

            ImGuiWindowFlags host_window_flags = 0;

            host_window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDocking;
            host_window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_MenuBar;
            host_window_flags |= ImGuiWindowFlags_NoBackground;


            char label[32];
            UIFormatString(label, IM_ARRAYSIZE(label), "DockSpaceViewport_%08X", viewport->ID);

            UIPushStyleVarF(ImGuiStyleVar_WindowRounding, 0);
            UIPushStyleVarF(ImGuiStyleVar_WindowBorderSize, 0);
            UIPushStyleVarV2(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));

            UIBegin(label, ElectronSignal_None, host_window_flags);
            UIPopStyleVarIter(3);
            if (instance->isNativeWindow) {
                DockspaceRenderTabBar(instance);
            }

            ImGuiID dockspace_id = UIGetID("DockSpace");
            UIDockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags, nullptr);

            UIEnd();
        };
    }

}