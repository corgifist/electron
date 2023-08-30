#include "editor_core.h"
#include "ui_api.h"

using namespace Electron;

extern "C" {
    ELECTRON_EXPORT void LayerPropertiesRender(AppInstance* instance) {
        UISetNextWindowSize({512, 512}, ImGuiCond_Once);
        UIBegin(CSTR(ELECTRON_GET_LOCALIZATION(instance, "LAYER_PROPERTIES_TITLE") + std::string("##") + std::to_string(CounterGetLayerProperties())), ElectronSignal_CloseWindow, 0);
            ImVec2 windowSize = UIGetWindowSize();
            if (!instance->projectOpened) {
                std::string projectWarningString = ELECTRON_GET_LOCALIZATION(instance, "LAYER_PROPERTIES_NO_PROJECT");
                ImVec2 warningSize = UICalcTextSize(projectWarningString.c_str());
                UISetCursorPos({windowSize.x / 2.0f - warningSize.x / 2.0f, windowSize.y / 2.0f - warningSize.y / 2.0f});
                UIText(CSTR(projectWarningString));
                UIEnd();
                return;
            }

            if (instance->selectedRenderLayer == -1 || instance->graphics.layers.size() == 0) {
                std::string noLayerWarning = ELECTRON_GET_LOCALIZATION(instance, "LAYER_PROPERTIES_NO_LAYER_SELECTED");
                ImVec2 warningSize = UICalcTextSize(CSTR(noLayerWarning));
                UISetCursorPos({windowSize.x / 2.0f - warningSize.x / 2.0f, windowSize.y / 2.0f - warningSize.y / 2.0f});
                UIText(CSTR(noLayerWarning));
                UIEnd();
                return;
            }

            RenderLayer* targetLayer = GraphicsImplGetLayerByID(&instance->graphics, instance->selectedRenderLayer);
            UIPushFont(instance->largeFont);
                UIText(CSTR(targetLayer->layerUsername + " (" + std::string(targetLayer->layerPublicName) + "<" + std::to_string(targetLayer->id) + ">" + ")"));
            UIPopFont();
            UISameLine();
            if (UIButton(string_format("%s %s", ICON_FA_COPY, ELECTRON_GET_LOCALIZATION(instance, "GENERIC_COPY_ID")).c_str())) {
                ClipSetText(std::to_string(targetLayer->id));
            }
            UISpacing();
            UIText(CSTR(std::string(ELECTRON_GET_LOCALIZATION(instance, "LAYER_PROPERTIES_DYNAMIC_LIBRARY")) + ": " + targetLayer->layerLibrary));
            UIText(CSTR(std::string(ELECTRON_GET_LOCALIZATION(instance, "LAYER_PROPERTIES_RENDER_BOUNDS")) + ": " + std::to_string(targetLayer->beginFrame) + " -> " + std::to_string(targetLayer->endFrame)));
            UIInputField(CSTR(std::string(ELECTRON_GET_LOCALIZATION(instance, "LAYER_PROPERTIES_LAYER_NAME"))), &targetLayer->layerUsername, 0);
            UISpacing();
            UISeparator();
            UISpacing();
            RenderLayerImplRenderProperties(targetLayer);
        UIEnd();
    }
}