#include "editor_core.h"
#include "app.h"
#include "shared.h"
#include "ImGuiFileDialog.h"
#define CSTR(x) ((x).c_str())

using namespace Electron;

extern "C" {

    void DockspaceRenderTabBar(AppInstance* instance, bool& openModal) {
        if (ImGui::BeginMenuBar()) {
                if (ImGui::BeginMenu(string_format("%s %s", ICON_FA_FOLDER_OPEN, ELECTRON_GET_LOCALIZATION("PROJECT_CONFIGURATION_MENU_BAR_PROJECT_MENU")).c_str())) {
                    if (ImGui::MenuItem(CSTR(ICON_FA_FOLDER_OPEN + std::string(" ") + ELECTRON_GET_LOCALIZATION("PROJECT_CONFIGURATION_MENU_BAR_PROJECT_MENU_OPEN")), "Ctrl+P+O")) {
                        ImGuiFileDialog::Instance()->OpenDialog("OpenProjectDialog", "Open project", nullptr, ".");
                    }
                    if (ImGui::MenuItem(CSTR(ICON_FA_FOLDER_OPEN + std::string(" ") + std::string(ELECTRON_GET_LOCALIZATION("PROJECT_CONFIGURATION_OPEN_RECENT_PROJECT")) + ": " + JSON_AS_TYPE(Shared::configMap["LastProject"], std::string)), "Ctrl+P+L", false, JSON_AS_TYPE(Shared::configMap["LastProject"], std::string) != "null")) {
                        ProjectMap project{};
                        project.path = Shared::configMap["LastProject"];
                        project.propertiesMap = json_t::parse(std::fstream(std::string(project.path) + "/project.json"));
                        Shortcuts::Ctrl_P_O(project);
                    }
                    ImGui::Separator();
                    if (ImGui::MenuItem(CSTR(ICON_FA_TRASH_CAN + std::string(" ") + std::string(ELECTRON_GET_LOCALIZATION("CLEAR_PROJECT_CACHES"))), "Ctrl+E+C")) {
                        openModal = true;
                    }
                    if (ImGui::MenuItem(CSTR(ICON_FA_SPINNER + std::string(" ") + std::string(ELECTRON_GET_LOCALIZATION("RELOAD_APPLICATION"))), "")) {
                        ImGui::EndMenu();
                        ImGui::EndMenuBar();
                        UI::End();
                        throw Signal::_ReloadSystem;
                    }
                    if (ImGui::MenuItem(CSTR(ICON_FA_XMARK + std::string(" ") + std::string(ELECTRON_GET_LOCALIZATION("PROJECT_CONFIGURAITON_MENU_BAR_PROJECT_MENU_EXIT"))), "Ctrl+P+E")) {
                        Shortcuts::Ctrl_P_E();
                    }
                    ImGui::EndMenu();
                }
                if (ImGui::BeginMenu(CSTR(ICON_FA_LAYER_GROUP + std::string(" ") + std::string(ELECTRON_GET_LOCALIZATION("PROJECT_CONFIGURATION_MENU_BAR_LAYERS"))))) {
                    auto registry = Shared::graphics->GetImplementationsRegistry();
                    for (auto& key : registry) {
                        Libraries::LoadLibrary("layers", key);
                        if (ImGui::MenuItem(string_format("%s %s (%s)", ICON_FA_PLUS, Libraries::GetVariable<std::string>(key, "LayerName").c_str(), key.c_str()).c_str())) {
                            Shared::graphics->AddRenderLayer(key);
                        }
                    }
                    ImGui::EndMenu();
                }
                if (ImGui::BeginMenu(CSTR(std::string(ICON_FA_TOOLBOX + std::string(" ") + ELECTRON_GET_LOCALIZATION("PROJECT_CONFIGURATION_MENU_BAR_WINDOW_MENU"))))) {
                    if (ImGui::MenuItem(string_format("%s %s", ICON_FA_IMAGE, ELECTRON_GET_LOCALIZATION("RENDER_PREVIEW_WINDOW_TITLE")).c_str(), "Ctrl+W+R", false, UICounters::RenderPreviewCounter != 1)) {
                        Shortcuts::Ctrl_W_R();
                    }
                    if (ImGui::MenuItem(string_format("%s %s", ICON_FA_LIST, ELECTRON_GET_LOCALIZATION("LAYER_PROPERTIES_TITLE")).c_str(), "Ctrl+W+L", false, UICounters::LayerPropertiesCounter != 1)) {
                        Shortcuts::Ctrl_W_L();
                    }
                    if (ImGui::MenuItem(string_format("%s %s", ICON_FA_FOLDER, ELECTRON_GET_LOCALIZATION("ASSET_MANAGER_TITLE")).c_str(), "Ctrl+W+A", false, UICounters::AssetManagerCounter != 1)) {
                        Shortcuts::Ctrl_W_A();
                    }
                    if (ImGui::MenuItem(string_format("%s %s", ICON_FA_MAGNIFYING_GLASS, ELECTRON_GET_LOCALIZATION("ASSET_MANAGER_ASSET_EXAMINER")).c_str(), "Ctrl+W+E", false, UICounters::AssetExaminerCounter != 1)) {
                        Shortcuts::Ctrl_W_E();
                    }
                    if (ImGui::MenuItem(string_format("%s %s", ICON_FA_TIMELINE, ELECTRON_GET_LOCALIZATION("TIMELINE_TITLE")).c_str(), "Ctrl+W+T", false, UICounters::TimelineCounter != 1)) {
                        Shortcuts::Ctrl_W_T();
                    }
                    ImGui::EndMenu();
                }
                ImGui::EndMenuBar();
            }
    }

    ELECTRON_EXPORT void UIRender(AppInstance* instance) {
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

            UI::Begin(label, Signal::_None, host_window_flags);
            ImGui::PopStyleVar(3);
            bool openModal = false;
            static bool modalActive = true;
            if (instance->isNativeWindow) {
                DockspaceRenderTabBar(instance, openModal);
            }
            if (openModal) {
                ImGui::OpenPopup(ELECTRON_GET_LOCALIZATION("ARE_YOU_SURE"));
                modalActive = true;
            }

            ImGuiID dockspace_id = ImGui::GetID("DockSpace");
            ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags, nullptr);

            if (ImGui::BeginPopupModal(ELECTRON_GET_LOCALIZATION("ARE_YOU_SURE"), &modalActive, ImGuiWindowFlags_AlwaysAutoResize)) {
                ImGui::Text("%s", ELECTRON_GET_LOCALIZATION("PROJECT_INVALIDATION_WARNING"));
                ImGui::Spacing();
                if (instance->ButtonCenteredOnLine(ELECTRON_GET_LOCALIZATION("GENERIC_OK"))) {
                    Shortcuts::Ctrl_E_C();
                    modalActive = false;
                };
                ImGui::EndPopup();
            }

            UI::End();
        };
    }

}