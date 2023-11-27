#include "shortcuts.h"

namespace Electron {
void Shortcuts::Ctrl_W_R() {
    Shared::app->ExecuteSignal(ElectronSignal_SpawnRenderPreview);
}

void Shortcuts::Ctrl_P_O(ProjectMap projectMap) {
    Shared::app->projectOpened = true;
    Shared::project = projectMap;

    Shared::selectedRenderLayer = JSON_AS_TYPE(projectMap.propertiesMap["LastSelectedLayer"], int);

    Shared::assets->LoadFromProject(projectMap.propertiesMap);
}

void Shortcuts::Ctrl_P_E() { throw ElectronSignal_CloseEditor; }

void Shortcuts::Ctrl_W_L() {
    Shared::app->ExecuteSignal(ElectronSignal_SpawnLayerProperties);
}

void Shortcuts::Ctrl_W_A() {
   Shared::app->ExecuteSignal(ElectronSignal_SpawnAssetManager);
}

void Shortcuts::Ctrl_W_T() {
    Shared::app->ExecuteSignal(ElectronSignal_SpawnTimeline);
}

void Shortcuts::Ctrl_E_C() {
    system("rm -r cache");
    system("mkdir cache");
}
}