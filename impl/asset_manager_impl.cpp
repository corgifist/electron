#include "editor_core.h"
#include "ui_api.h"

using namespace Electron;

extern "C" {
    ELECTRON_EXPORT void AssetManagerRender(AppInstance* instance) {
        UIBegin(ELECTRON_GET_LOCALIZATION(instance, "ASSET_MANAGER_TITLE"), ElectronSignal_CloseWindow, 0);
            ImVec2 windowSize = UIGetWindowSize();

            static std::string importErrorMessage = "";

            float totalResourcesSize = 0;
            for (auto& asset : instance->assets.assets) {
                if (asset.invalid) continue;
                totalResourcesSize += filesize(CSTR(asset.path));
            }
            if (UIButton(ELECTRON_GET_LOCALIZATION(instance, "ASSET_MANAGER_IMPORT_ASSET"))) {
                FileDialogImplOpenDialog("ImportAssetDlg", ELECTRON_GET_LOCALIZATION(instance, "ASSET_MANAGER_SELECT_ASSET_PATH"), IMPORT_EXTENSIONS, ".");
            }
            UISameLine();
            UIText(CSTR(std::string(ELECTRON_GET_LOCALIZATION(instance, "ASSET_MANAGER_ASSETS_SIZE_IN_RAM")) + ": " + std::to_string(totalResourcesSize / 1024.0f / 1024.0f) + " MB"));
            UISameLine();
            UIText(CSTR("  " + std::string(ELECTRON_GET_LOCALIZATION(instance, "ASSET_MANAGER_TOTAL_ASSET_COUNT")) + ": " + std::to_string(instance->assets.assets.size())));
            UISeparator();
            int assetIndex = 0;
            int assetDeletionIndex = -1;
            for (auto& asset : instance->assets.assets) {
                UIPushColor(ImGuiCol_Button, ImVec4{1, 0, 0, 1});
                    if (UIButton(CSTR(ELECTRON_GET_LOCALIZATION(instance, "GENERIC_DELETE") + std::string("##") + std::to_string((uint64_t) &asset)))) {
                        assetDeletionIndex = assetIndex;
                    }
                UIPopColor();
                UISameLine();
                if (UICollapsingHeader(CSTR("[" + asset.strType + "] " + asset.name + "###" + std::to_string((uint64_t) &asset)))) {
                    // render image preview
                    switch (asset.type) {
                        case TextureUnionType::Texture: {
                            PixelBuffer& pbo = std::get<PixelBuffer>(asset.as);

                            UISetCursorX((windowSize.x / 2.0f) - ((pbo.width * asset.previewScale) / 2.0f));
                            UIImage(asset.pboGpuTexture, ImVec2{pbo.width * asset.previewScale, pbo.height * asset.previewScale});
                            print(asset.pboGpuTexture);
                            UISliderFloat(CSTR(ELECTRON_GET_LOCALIZATION(instance, "ASSET_MANAGER_TEXTURE_PREVIEW_SCALE") + std::string("##") + std::to_string((uint64_t) &asset)), &asset.previewScale, 0.1f, 2.0f, "%0.1f", 0);
                            UIText(CSTR(ELECTRON_GET_LOCALIZATION(instance, "ASSET_MANAGER_TEXTURE_RESOLUTION") + std::string(": ") + std::to_string(pbo.width) + "x" + std::to_string(pbo.height)));
                            break;
                        }
                    }

                    std::string assetName = asset.name;
                    if (UIInputField(CSTR(ELECTRON_GET_LOCALIZATION(instance, "ASSET_MANAGER_RESOURCE_NAME") + std::string("##") + std::to_string((uint64_t) &asset)), &assetName, ImGuiInputTextFlags_EnterReturnsTrue)) {
                        asset.name = assetName;
                    }

                    std::string reservedResourcePath = asset.path;
                    std::string assetPath = asset.path;
                    if (UIInputField(CSTR(std::string(ELECTRON_GET_LOCALIZATION(instance, "ASSET_MANAGER_RESOURCE_PATH")) + (asset.invalid ? std::string(" (") + ELECTRON_GET_LOCALIZATION(instance, "ASSET_MANAGER_INVALID_RESOURCE_PATH") + ")" : "") + "##" + std::to_string((uint64_t) &asset)), &assetPath, ImGuiInputTextFlags_EnterReturnsTrue)) {
                        asset.path = assetPath;
                    }
                    if (reservedResourcePath != asset.path) {
                        TextureUnionImplRebuildAssetData(&asset, &instance->graphics);
                    }
                }
                assetIndex++;
            }
            if (assetDeletionIndex != -1) {
                auto& assets = instance->assets.assets;
                assets.erase(assets.begin() + assetDeletionIndex);
            }
        UIEnd();

        if (importErrorMessage != "") {
            UISetNextWindowSize(ImVec2{640, 128}, ImGuiCond_Always);
            UIBegin(ELECTRON_GET_LOCALIZATION(instance, "ASSET_MANAGER_IMPORT_FAILURE"), ElectronSignal_None, ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);
                UISetCursorX(windowSize.x / 2.0f - UICalcTextSize(CSTR(importErrorMessage)).x / 2.0f);
                UIText(CSTR(importErrorMessage));

                if (UICenteredButton(instance, ELECTRON_GET_LOCALIZATION(instance, "GENERIC_OK"))) {
                    importErrorMessage = "";
                }
            UIEnd();
        }

        if (FileDialogImplDisplay("ImportAssetDlg")) {
            if (FileDialogImplIsOK()) {
                importErrorMessage = AssetManagerImplImportAsset(&instance->assets, FileDialogImplGetFilePathName());
            }
            FileDialogImplClose();
        }
    }
}