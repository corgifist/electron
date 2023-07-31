#include "editor_core.h"

dylib Electron::ImplDylibs::AssetManagerDylib{};

Electron::AssetManager::AssetManager() {
    this->impl = ImplDylibs::AssetManagerDylib.get_function<void(AppInstance*)>("AssetManagerRender");
}

void Electron::AssetManager::Render(AppInstance* instance) {
    impl(instance);
}
