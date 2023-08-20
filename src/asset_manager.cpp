#include "editor_core.h"

namespace Electron {
    dylib ImplDylibs::AssetManagerDylib{};
    int UICounters::AssetManagerCounter;

    AssetManager::AssetManager() {
        this->impl = ImplDylibs::AssetManagerDylib.get_function<void(AppInstance*)>("AssetManagerRender");
        UICounters::AssetManagerCounter++;
    }

    AssetManager::~AssetManager() {
        UICounters::AssetManagerCounter--;
    }

    void AssetManager::Render(AppInstance* instance) {
        impl(instance);
    }
}
