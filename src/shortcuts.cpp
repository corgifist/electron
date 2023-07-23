#include "shortcuts.h"

void Electron::Shortcuts::Ctrl_W_R() {
    owner->ExecuteSignal(ElectronSignal_SpawnRenderPreview);
}

void Electron::Shortcuts::Ctrl_P_O(ProjectMap projectMap) {
    owner->projectOpened = true;
    owner->project = projectMap;
}
        
void Electron::Shortcuts::Ctrl_P_E() {
    throw ElectronSignal_CloseEditor;
}

void Electron::Shortcuts::Ctrl_W_L() {
    owner->ExecuteSignal(ElectronSignal_SpawnLayerProperties);
}