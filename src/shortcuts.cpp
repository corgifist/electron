#include "shortcuts.h"

void Electron::Shortcuts::Ctrl_W_R() {
    owner->ExecuteSignal(ElectronSignal_SpawnRenderPreview);
}

void Electron::Shortcuts::Ctrl_P_O(ProjectMap projectMap) {
    owner->projectOpened = true;
    owner->project = projectMap;
    owner->graphics.projectPath = projectMap.path;

    owner->assets.LoadFromProject(projectMap.propertiesMap);
}
        
void Electron::Shortcuts::Ctrl_P_E() {
    throw ElectronSignal_CloseEditor;
}

void Electron::Shortcuts::Ctrl_W_L() {
    owner->ExecuteSignal(ElectronSignal_SpawnLayerProperties);
}

void Electron::Shortcuts::Ctrl_W_A() {
    owner->ExecuteSignal(ElectronSignal_SpawnAssetManager);
}

void Electron::Shortcuts::Ctrl_W_T() {
    owner->ExecuteSignal(ElectronSignal_SpawnTimeline);
}