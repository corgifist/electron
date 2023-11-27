#include "editor_core.h"
#include "app.h"
#include "shared.h"
#include "build_number.h"

#include "ImGuiFileDialog.h"

#define CSTR(x) ((x).c_str())

using namespace Electron;

extern "C" {


    ELECTRON_EXPORT void ProjectConfigurationRender(AppInstance* instance) {
        ImGui::SetCurrentContext(instance->context);
        static bool dockInitialized = false;

        // UIDockSpaceOverViewport(UIGetViewport(), ImGuiDockNodeFlags_PassthruCentralNode, nullptr);


        ImGuiWindowFlags dockFlags = ImGuiWindowFlags_NoCollapse;
        if (instance->isNativeWindow) {

            ImVec2 displaySize = ImGui::GetIO().DisplaySize;
            ImGui::SetNextWindowSize({640, 480}, ImGuiCond_Once);
        } else {
            dockFlags |= ImGuiWindowFlags_MenuBar;
            ImGui::SetNextWindowSize({640, 480}, ImGuiCond_Once);
        }
 
        UI::Begin(CSTR(std::string(ICON_FA_SCREWDRIVER " ") + ELECTRON_GET_LOCALIZATION("PROJECT_CONFIGURATION_WINDOW_TITLE") + std::string("##") + std::to_string(UICounters::ProjectConfigurationCounter)), instance->isNativeWindow ? ElectronSignal_CloseWindow : ElectronSignal_CloseEditor, dockFlags);
            std::string projectTip = ELECTRON_GET_LOCALIZATION("PROJECT_CONFIGURATION_CREATE_PROJECT_TIP");
            ImVec2 windowSize = ImGui::GetWindowSize();
            ImVec2 tipSize = ImGui::CalcTextSize(projectTip.c_str());
    

        if (!instance->isNativeWindow) (instance);        

        if (ImGui::BeginTabBar(ELECTRON_GET_LOCALIZATION("PROJECT_CONFIGURATION_CONFIGURATIONS"), 0)) {
            if (ImGui::BeginTabItem(ELECTRON_GET_LOCALIZATION("PROJECT_CONFIGURATION_PROJECT_CONFIGURATION"), nullptr, 0)) {
            if (instance->projectOpened) {
                ProjectMap& project = Shared::project;
                std::string projectConfigurationTitle = string_format("%s %s", ICON_FA_GEAR, ELECTRON_GET_LOCALIZATION("PROJECT_CONFIGURATION_PROJECT_CONFIGURATION"));

                ImGui::PushFont(instance->largeFont);
                    ImVec2 titleSize = ImGui::CalcTextSize(CSTR(projectConfigurationTitle));
                    ImGui::SetCursorPosX(windowSize.x / 2.0f - titleSize.x / 2.0f);
                    ImGui::Text("%s", CSTR(projectConfigurationTitle));
                ImGui::PopFont();
                ImGui::Separator();

                std::string name = JSON_AS_TYPE(project.propertiesMap["ProjectName"], std::string);
                ImGui::InputText(ELECTRON_GET_LOCALIZATION("PROJECT_CONFIGURATION_PROJECT_NAME_LABEL"), &name, 0);
                project.propertiesMap["ProjectName"] = name;

                std::vector<int> sourceResolution = JSON_AS_TYPE(project.propertiesMap["ProjectResolution"], std::vector<int>);
                std::vector<int> resolutionPtr = sourceResolution;
                ImGui::InputInt2(ELECTRON_GET_LOCALIZATION("PROJECT_CONFIGURATION_RESOLUTION_LABEL"), resolutionPtr.data(), 0);
                project.propertiesMap["ProjectResolution"] = resolutionPtr;

                std::vector<float> backgroundColor = JSON_AS_TYPE(project.propertiesMap["BackgroundColor"], std::vector<float>);
                ImGui::ColorEdit3(ELECTRON_GET_LOCALIZATION("PROJECT_CONFIGURATION_BACKGROUND_COLOR"), backgroundColor.data(), 0);
                project.propertiesMap["BackgroundColor"] = backgroundColor;

                int projectFramerate = JSON_AS_TYPE(project.propertiesMap["Framerate"], int);
                float fProjectFramerate = (float) projectFramerate;
                ImGui::SliderFloat(ELECTRON_GET_LOCALIZATION("PROJECT_CONFIGURATION_FRAMERATE"), &fProjectFramerate, 1, 60, "%0.0f", 0);
                projectFramerate = (int) fProjectFramerate;
                project.propertiesMap["Framerate"] = projectFramerate;
            }

            if (!instance->projectOpened) {
                ImGui::SetCursorPos(ImVec2{
                    windowSize.x / 2.0f - tipSize.x / 2.0f,
                    windowSize.y / 2.0f - tipSize.y / 2.0f
                });
                ImGui::Text("%s", projectTip.c_str());
            }
            ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem(ELECTRON_GET_LOCALIZATION("PROJECT_CONFIGURATION_EDITOR_CONFIGURATION"), nullptr, 0)) {
                ImGui::PushFont(instance->largeFont);
                    std::string editorConfigurationString = string_format("%s %s", ICON_FA_GEAR, ELECTRON_GET_LOCALIZATION("PROJECT_CONFIGURATION_EDITOR_CONFIGURATION"));
                    ImVec2 editorConfigurationTextSize = ImGui::CalcTextSize(CSTR(editorConfigurationString));
                    ImGui::SetCursorPosX(windowSize.x / 2.0f - editorConfigurationTextSize.x / 2.0f);
                    ImGui::Text("%s", CSTR(editorConfigurationString));
                ImGui::PopFont();
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("%s %s: %i", ICON_FA_INFO, ELECTRON_GET_LOCALIZATION("BUILD_NUMBER"), BUILD_NUMBER);
                }
                ImGui::Separator();

                bool usingLinearFiltering = JSON_AS_TYPE(Shared::configMap["TextureFiltering"], std::string) == "linear";
                static std::string textureFilters[] = {
                    ELECTRON_GET_LOCALIZATION("PROJECT_CONFIGURATION_LINEAR"),
                    ELECTRON_GET_LOCALIZATION("PROJECT_CONFIGURATION_NEAREST")
                };
                static int selectedTextureFiltering = usingLinearFiltering ? 0 : 1;
                if (ImGui::BeginCombo(ELECTRON_GET_LOCALIZATION("PROJECT_CONFIGURATION_TEXTURE_FILTERING"), CSTR(textureFilters[selectedTextureFiltering]))) {
                    for (int i = 0; i < 2; i++) {
                        bool filterSelected = (i == selectedTextureFiltering);
                        if (ImGui::Selectable(CSTR(textureFilters[i]), filterSelected)) {
                            selectedTextureFiltering = i;
                        }
                        if (filterSelected) ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }

                Shared::configMap["TextureFiltering"] = selectedTextureFiltering == 0 ? "linear" : "nearest";
                
                int timelinePrescision = JSON_AS_TYPE(Shared::configMap["RenderPreviewTimelinePrescision"], int);
                timelinePrescision = std::clamp(timelinePrescision, 0, 1000);
                ImGui::InputInt(ELECTRON_GET_LOCALIZATION("PROJECT_CONFIGURATION_RENDER_PREVIEW_TIMELINE_PRESCISION"), &timelinePrescision, 1, 100, 0);
                Shared::configMap["RenderPreviewTimelinePrescision"] = timelinePrescision;

                bool resizeInterpolation = JSON_AS_TYPE(Shared::configMap["ResizeInterpolation"], bool);
                ImGui::Checkbox(ELECTRON_GET_LOCALIZATION("PROJECT_CONFIGURATION_RESIZE_INTERPOLATION"), &resizeInterpolation);
                Shared::configMap["ResizeInterpolation"] = resizeInterpolation;
                if (ImGui::IsItemHovered()) 
                    ImGui::SetTooltip("%s", ELECTRON_GET_LOCALIZATION("PROJECT_CONFIGURATION_SMOOTHNESS_DECREASE"));

                float uiScaling = JSON_AS_TYPE(Shared::configMap["UIScaling"], float);
                ImGui::SliderFloat(ELECTRON_GET_LOCALIZATION("PROJECT_CONFIGURATION_EDITOR_UI_SCALING"), &uiScaling, 0.5f, 2.5f, "%0.1f", 0);
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("%s", ELECTRON_GET_LOCALIZATION("PROJECT_CONFIGURATION_RESTART_REQUIRED"));
                Shared::configMap["UIScaling"] = uiScaling;

                bool preferX11 = JSON_AS_TYPE(Shared::configMap["PreferX11"], bool);
                ImGui::Checkbox(ELECTRON_GET_LOCALIZATION("PREFER_X11_SESSION"), &preferX11);
                Shared::configMap["PreferX11"] = preferX11;

                ImGui::Separator();
                ImGui::Spacing();
                ImGui::Text("GL_RENDERER: %s", instance->renderer.c_str());
                ImGui::Text("GL_VENDOR: %s", instance->vendor.c_str());
                ImGui::Text("GL_VERSION: %s", instance->version.c_str());
                
            ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
        UI::End();

        if (ImGuiFileDialog::Instance()->Display("OpenProjectDialog")) {
            if (ImGuiFileDialog::Instance()->IsOk()) {
                ProjectMap project{};
                project.path = ImGuiFileDialog::Instance()->GetFilePathName();
                project.propertiesMap = json_t::parse(std::fstream(std::string(project.path) + "/project.json"));
                instance->shortcuts.Ctrl_P_O(project);
            }

            ImGuiFileDialog::Instance()->Close();
        }
    }
}