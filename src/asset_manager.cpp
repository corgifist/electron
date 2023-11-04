#include "editor_core.h"

namespace Electron {
    std::string ImplDylibs::AssetManagerDylib = "asset_manager_impl";
    int UICounters::AssetManagerCounter;

    AssetManager::AssetManager() {
        this->impl =
            Libraries::GetFunction<void(AppInstance *)>(
                ImplDylibs::AssetManagerDylib, "AssetManagerRender");
        UICounters::AssetManagerCounter = 1;
    }

    AssetManager::~AssetManager() { UICounters::AssetManagerCounter = 0; }

    void AssetManager::Render(AppInstance *instance) { impl(instance); }
} // namespace Electron
