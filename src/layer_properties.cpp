#include "editor_core.h"

void Electron::LayerProperties::Render(AppInstance* instance) {
    bool pOpen = true;
    ImGui::SetNextWindowSize({512, 512}, ImGuiCond_Once);
    ImGui::Begin(std::string(std::string(ELECTRON_GET_LOCALIZATION(instance, "LAYER_PROPERTIES_TITLE")) + "##" + std::to_string((uint64_t) this)).c_str(), &pOpen, 0);
    ImVec2 windowSize = ImGui::GetWindowSize();
    if (!pOpen) {
        ImGui::End();
        throw ElectronSignal_CloseWindow;
    }
    if (!instance->projectOpened) {
        std::string projectWarningString = ELECTRON_GET_LOCALIZATION(instance, "LAYER_PROPERTIES_NO_PROJECT");
        ImVec2 warningSize = ImGui::CalcTextSize(projectWarningString.c_str());
        ImGui::SetCursorPos({windowSize.x / 2.0f - warningSize.x / 2.0f, windowSize.y / 2.0f - warningSize.y / 2.0f});
        ImGui::Text(projectWarningString.c_str());
        ImGui::End();
        return;
    }

    if (instance->selectedRenderLayer = -1) {
        std::string noLayerWarning = ELECTRON_GET_LOCALIZATION(instance, "LAYER_PROPERTIES_NO_LAYER_SELECTED");
        ImVec2 warningSize = ImGui::CalcTextSize(noLayerWarning.c_str());
        ImGui::SetCursorPos({windowSize.x / 2.0f - warningSize.x / 2.0f, windowSize.y / 2.0f - warningSize.y / 2.0f});
        ImGui::Text(noLayerWarning.c_str());
        ImGui::End();
        return;
    }

    RenderLayer* targetLayer = instance->graphics.layers.data() + instance->selectedRenderLayer;
    ImGui::PushFont(instance->largeFont);
        ImGui::Text((std::string(targetLayer->layerPublicName) + "<" + std::to_string(instance->selectedRenderLayer) + ">").c_str());
    ImGui::PopFont();
    ImGui::Separator();
    ImGui::End();
}