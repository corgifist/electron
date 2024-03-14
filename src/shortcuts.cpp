#include "shortcuts.h"

namespace Electron {
void Shortcuts::Ctrl_W_R() {
    AppCore::ExecuteSignal(Signal::_SpawnRenderPreview);
}

void Shortcuts::Ctrl_P_O(ProjectMap projectMap) {
    if (AppCore::projectOpened) {
        for (auto& asset : AssetCore::assets) {
            asset.Destroy();
        }
        AssetCore::assets.clear();
        for (auto& layer : GraphicsCore::layers) {
            layer.Destroy();
        }
        GraphicsCore::layers.clear();
    }
    AppCore::projectOpened = true;
    Shared::project = projectMap;

    Shared::selectedRenderLayer =
        JSON_AS_TYPE(projectMap.propertiesMap["LastSelectedLayer"], int);
    Shared::maxFramerate =
        JSON_AS_TYPE(projectMap.propertiesMap["MaxFramerate"], float);

    AssetCore::LoadFromProject(projectMap.propertiesMap);
}

void Shortcuts::Ctrl_P_E() { throw Signal::_CloseEditor; }

void Shortcuts::Ctrl_W_L() {
    AppCore::ExecuteSignal(Signal::_SpawnLayerProperties);
}

void Shortcuts::Ctrl_W_A() {
    AppCore::ExecuteSignal(Signal::_SpawnAssetManager);
}

void Shortcuts::Ctrl_W_T() {
    AppCore::ExecuteSignal(Signal::_SpawnTimeline);
}

void Shortcuts::Ctrl_W_E() {
    AppCore::ExecuteSignal(Signal::_SpawnAssetExaminer);
}

void Shortcuts::Ctrl_E_C() {
    if (system("rm -r cache")) {}
    if (system("mkdir cache")) {}
}
} // namespace Electron