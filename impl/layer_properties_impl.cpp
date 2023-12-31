#include "editor_core.h"
#include "app.h"
#define CSTR(x) ((x).c_str())

using namespace Electron;

extern "C" {
    ELECTRON_EXPORT void UIRender(AppInstance* instance) {
        ImGui::SetCurrentContext(instance->context);
        ImGui::SetNextWindowSize({512, 512}, ImGuiCond_Once);
        UI::Begin(CSTR(std::string(ICON_FA_LIST " ") + ELECTRON_GET_LOCALIZATION("LAYER_PROPERTIES_TITLE") + std::string("##") + std::to_string(UICounters::LayerPropertiesCounter)), Signal::_CloseWindow, ImGuiWindowFlags_NoScrollWithMouse);
            ImVec2 windowSize = ImGui::GetContentRegionAvail();
            if (!instance->projectOpened) {
                std::string projectWarningString = ELECTRON_GET_LOCALIZATION("LAYER_PROPERTIES_NO_PROJECT");
                ImVec2 warningSize = ImGui::CalcTextSize(projectWarningString.c_str());
                ImGui::SetCursorPos({windowSize.x / 2.0f - warningSize.x / 2.0f, windowSize.y / 2.0f - warningSize.y / 2.0f});
                ImGui::Text("%s", CSTR(projectWarningString));
                ImGui::End();
                return;
            }

            static bool setup = false;
            if (!setup) {
                Shared::selectedRenderLayer = JSON_AS_TYPE(Shared::project.propertiesMap["LastSelectedLayer"], int);
                setup = true;
            }

            bool layerExists = false;
            if (instance->projectOpened) {
                try {
                    Shared::graphics->GetLayerByID(Shared::selectedRenderLayer);
                    layerExists = true;
                } catch (const std::runtime_error& err) {
                    layerExists = false;
                }
            }
            if (Shared::selectedRenderLayer == -1 || Shared::graphics->layers.size() == 0 || !layerExists) {
                std::string noLayerWarning = ELECTRON_GET_LOCALIZATION("LAYER_PROPERTIES_NO_LAYER_SELECTED");
                ImVec2 warningSize = ImGui::CalcTextSize(CSTR(noLayerWarning));
                ImGui::SetCursorPos({windowSize.x / 2.0f - warningSize.x / 2.0f, windowSize.y / 2.0f - warningSize.y / 2.0f});
                ImGui::Text("%s", CSTR(noLayerWarning));
                UI::End();
                return;
            }

            static float titleChildHeight = 100;
            RenderLayer* targetLayer = Shared::graphics->GetLayerByID(Shared::selectedRenderLayer);
            ImGui::BeginChild("layerPropsTitleChild", ImVec2(ImGui::GetWindowSize().x, titleChildHeight), false, ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoScrollbar);
            float beginCursor = ImGui::GetCursorPos().y;
            ImGui::PushFont(instance->largeFont);
                ImGui::Text("%s", CSTR(std::string(ICON_FA_LAYER_GROUP " ") + targetLayer->layerUsername + " (" + std::string(targetLayer->layerPublicName) + "<" + std::to_string(targetLayer->id) + ">" + ")"));
            ImGui::PopFont();
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("%s", ELECTRON_GET_LOCALIZATION("LAYER_PROPERTIES_RIGHT_CLICK_FOR_INFO"));
            }
            if (ImGui::IsItemHovered() && ImGui::GetIO().MouseDown[ImGuiMouseButton_Right]) {
                ImGui::OpenPopup("layerPropAdditionalInfo", 0);
            }
            if (ImGui::BeginPopup("layerPropAdditionalInfo", 0)) {
                ImGui::SeparatorText(CSTR(string_format("%s %s", ICON_FA_CIRCLE_INFO, CSTR(targetLayer->layerPublicName))));
                ImGui::Text("%s", CSTR(std::string(ELECTRON_GET_LOCALIZATION("LAYER_PROPERTIES_DYNAMIC_LIBRARY")) + ": " + targetLayer->layerLibrary));
                ImGui::Text("%s", CSTR(std::string(ELECTRON_GET_LOCALIZATION("LAYER_PROPERTIES_RENDER_BOUNDS")) + ": " + std::to_string(targetLayer->beginFrame) + " -> " + std::to_string(targetLayer->endFrame)));
                ImGui::InputText(CSTR(std::string(ELECTRON_GET_LOCALIZATION("LAYER_PROPERTIES_LAYER_NAME"))), &targetLayer->layerUsername, 0);
                ImGui::EndPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button(string_format("%s %s", ICON_FA_COPY, ELECTRON_GET_LOCALIZATION("GENERIC_COPY_ID")).c_str())) {
                ImGui::SetClipboardText(CSTR(std::to_string(targetLayer->id)));
            }
            titleChildHeight = ImGui::GetCursorPos().y - beginCursor;
            ImGui::EndChild();
            ImGui::Separator();
            ImGui::BeginChild("layerPropertiesChild", ImVec2(ImGui::GetWindowSize().x, ImGui::GetWindowSize().y - ImGui::GetCursorPosY()), false, ImGuiWindowFlags_AlwaysVerticalScrollbar);
            targetLayer->RenderProperties();
            ImGui::EndChild();
        UI::End();
    }
}