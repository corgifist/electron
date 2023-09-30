#include "editor_core.h"
#include "app.h"
#include "ImGuiFileDialog.h"
#define CSTR(x) ((x).c_str())

using namespace Electron;

extern "C" {

    void DockspaceRenderTabBar(AppInstance* instance) {
        if (ImGui::BeginMenuBar()) {
                if (ImGui::BeginMenu(string_format("%s %s", ICON_FA_FOLDER_OPEN, ELECTRON_GET_LOCALIZATION(instance, "PROJECT_CONFIGURATION_MENU_BAR_PROJECT_MENU")).c_str())) {
                    if (ImGui::MenuItem(CSTR(ICON_FA_FOLDER_OPEN + std::string(" ") + ELECTRON_GET_LOCALIZATION(instance, "PROJECT_CONFIGURATION_MENU_BAR_PROJECT_MENU_OPEN")), "Ctrl+P+O")) {
                        ImGuiFileDialog::Instance()->OpenDialog("OpenProjectDialog", "Open project", nullptr, ".");
                    }
                    if (ImGui::MenuItem(CSTR(ICON_FA_FOLDER_OPEN + std::string(" ") + std::string(ELECTRON_GET_LOCALIZATION(instance, "PROJECT_CONFIGURATION_OPEN_RECENT_PROJECT")) + ": " + JSON_AS_TYPE(instance->configMap["LastProject"], std::string)), "Ctrl+P+L", false, JSON_AS_TYPE(instance->configMap["LastProject"], std::string) != "null")) {
                        ProjectMap project{};
                        project.path = instance->configMap["LastProject"];
                        project.propertiesMap = json_t::parse(std::fstream(std::string(project.path) + "/project.json"));
                        instance->shortcuts.Ctrl_P_O(project);
                    }
                    ImGui::Separator();
                    if (ImGui::MenuItem(CSTR(ICON_FA_RECYCLE + std::string(" ") + std::string(ELECTRON_GET_LOCALIZATION(instance, "RELOAD_APPLICATION"))), "")) {
                        ImGui::EndMenu();
                        ImGui::EndMenuBar();
                        UI::End();
                        throw ElectronSignal_ReloadSystem;
                    }
                    if (ImGui::MenuItem(CSTR(ICON_FA_CROSS + std::string(" ") + std::string(ELECTRON_GET_LOCALIZATION(instance, "PROJECT_CONFIGURAITON_MENU_BAR_PROJECT_MENU_EXIT"))), "Ctrl+P+E")) {
                        instance->shortcuts.Ctrl_P_E();
                    }
                    ImGui::EndMenu();
                }
                if (ImGui::BeginMenu(CSTR(ICON_FA_LAYER_GROUP + std::string(" ") + std::string(ELECTRON_GET_LOCALIZATION(instance, "PROJECT_CONFIGURATION_MENU_BAR_LAYERS"))))) {
                    auto registry = instance->graphics.GetImplementationsRegistry();
                    for (auto& entry : registry) {
                        std::string key = entry.first;

                        if (ImGui::MenuItem(registry.at(key).get_variable<std::string>("LayerName").c_str(), "")) {
                            instance->graphics.AddRenderLayer(key);
                        }
                    }
                    ImGui::EndMenu();
                }
                if (ImGui::BeginMenu(CSTR(std::string(ICON_FA_TOOLBOX + std::string(" ") + ELECTRON_GET_LOCALIZATION(instance, "PROJECT_CONFIGURATION_MENU_BAR_WINDOW_MENU"))))) {
                    if (ImGui::MenuItem(ELECTRON_GET_LOCALIZATION(instance, "PROJECT_CONFIGURATION_MENU_BAR_WINDOW_MENU_RENDER_PREVIEW"), "Ctrl+W+R", false, UICounters::RenderPreviewCounter != 1)) {
                        instance->shortcuts.Ctrl_W_R();
                    }
                    if (ImGui::MenuItem(ELECTRON_GET_LOCALIZATION(instance, "LAYER_PROPERTIES_TITLE"), "Ctrl+W+L", false, UICounters::LayerPropertiesCounter != 1)) {
                        instance->shortcuts.Ctrl_W_L();
                    }
                    if (ImGui::MenuItem(ELECTRON_GET_LOCALIZATION(instance, "ASSET_MANAGER_TITLE"), "Ctrl+W+A", false, UICounters::AssetManagerCounter != 1)) {
                        instance->shortcuts.Ctrl_W_A();
                    }
                    if (ImGui::MenuItem(ELECTRON_GET_LOCALIZATION(instance, "TIMELINE_TITLE"), "Ctrl+W+T", false, UICounters::TimelineCounter != 1)) {
                        instance->shortcuts.Ctrl_W_T();
                    }
                    ImGui::EndMenu();
                }
                ImGui::EndMenuBar();
            }
    }

    ELECTRON_EXPORT void DockspaceRender(AppInstance* instance) {
          {
            ImGui::SetCurrentContext(instance->context);
            ImGuiViewport* viewport = ImGui::GetWindowViewport();
            ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_PassthruCentralNode;

            ImGui::SetNextWindowPos(viewport->WorkPos, 0);
            ImGui::SetNextWindowSize(viewport->WorkSize, 0);
            ImGui::SetNextWindowViewport(viewport->ID);

            ImGuiWindowFlags host_window_flags = 0;

            host_window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDocking;
            host_window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_MenuBar;
            host_window_flags |= ImGuiWindowFlags_NoBackground;


            char label[32];
            ImFormatString(label, IM_ARRAYSIZE(label), "DockSpaceViewport_%08X", viewport->ID);

            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));

            UI::Begin(label, ElectronSignal_None, host_window_flags);
            ImGui::PopStyleVar(3);
            if (instance->isNativeWindow) {
                DockspaceRenderTabBar(instance);
            }

            ImGuiID dockspace_id = ImGui::GetID("DockSpace");
            ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags, nullptr);

            UI::End();
        };
    }

}