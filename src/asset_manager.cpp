#include "editor_core.h"

dylib Electron::ImplDylibs::AssetManagerDylib{};
int Electron::UICounters::AssetManagerCounter;

Electron::AssetManager::AssetManager() {
    this->impl = ImplDylibs::AssetManagerDylib.get_function<void(AppInstance*)>("AssetManagerRender");
    Electron::UICounters::AssetManagerCounter++;
}

void Electron::AssetManager::Render(AppInstance* instance) {
    impl(instance);
}
