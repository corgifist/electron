#include "editor_core.h"
#include "ui_api.h"

using namespace Electron;

extern "C" {
    ELECTRON_EXPORT void AssetManagerRender(AppInstance* instance) {
        UIBegin(ELECTRON_GET_LOCALIZATION(instance, "ASSET_MANAGER_TITLE"), ElectronSignal_CloseWindow, 0);
            UIText(ELECTRON_GET_LOCALIZATION(instance, "GENERIC_PLACEHOLDER"));
        UIEnd();
    }
}