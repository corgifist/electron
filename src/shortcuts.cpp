#include "shortcuts.h"

namespace Electron {
void Shortcuts::Ctrl_W_R() {
    Shared::app->ExecuteSignal(Signal::_SpawnRenderPreview);
}

void Shortcuts::Ctrl_P_O(ProjectMap projectMap) {
    Shared::app->projectOpened = true;
    Shared::project = projectMap;

    Shared::selectedRenderLayer = JSON_AS_TYPE(projectMap.propertiesMap["LastSelectedLayer"], int);

    Shared::assets->LoadFromProject(projectMap.propertiesMap);
}

void Shortcuts::Ctrl_P_E() { throw Signal::_CloseEditor; }

void Shortcuts::Ctrl_W_L() {
    Shared::app->ExecuteSignal(Signal::_SpawnLayerProperties);
}

void Shortcuts::Ctrl_W_A() {
   Shared::app->ExecuteSignal(Signal::_SpawnAssetManager);
}

void Shortcuts::Ctrl_W_T() {
    Shared::app->ExecuteSignal(Signal::_SpawnTimeline);
}

void Shortcuts::Ctrl_W_E() {
    Shared::app->ExecuteSignal(Signal::_SpawnAssetExaminer);
}

void Shortcuts::Ctrl_E_C() {
    system("rm -r cache");
    system("mkdir cache");
}
}