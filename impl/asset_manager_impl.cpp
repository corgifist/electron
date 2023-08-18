#include "editor_core.h"
#include "ui_api.h"

using namespace Electron;

extern "C" {
    ELECTRON_EXPORT void AssetManagerRender(AppInstance* instance) {
        UIBegin(CSTR(ELECTRON_GET_LOCALIZATION(instance, "ASSET_MANAGER_TITLE") + std::string("##") + std::to_string(CounterGetAssetManager())), ElectronSignal_CloseWindow, 0);
            ImVec2 windowSize = UIGetWindowSize();

            static std::string importErrorMessage = "";
            static int targetAssetBrowsePath = -1;

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
            static std::string searchFilter = "";
            UIInputField(ELECTRON_GET_LOCALIZATION(instance, "ASSET_MANAGER_SEARCH"), &searchFilter, 0);
            UISeparator();
            int assetIndex = 0;
            int assetDeletionIndex = -1;
            for (auto& asset : instance->assets.assets) {
                if (searchFilter != "" && asset.name.find(searchFilter) == std::string::npos) {
                    assetIndex++;
                    continue;
                }
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
                            glm::vec2 textureDimensions = TextureUnionImplGetDimensions(&asset);
                            UISetCursorX((windowSize.x / 2.0f) - ((textureDimensions.x * asset.previewScale) / 2.0f));
                            UIImage(asset.pboGpuTexture, ImVec2(textureDimensions.x, textureDimensions.y) * asset.previewScale);
                            UISliderFloat(CSTR(ELECTRON_GET_LOCALIZATION(instance, "ASSET_MANAGER_TEXTURE_PREVIEW_SCALE") + std::string("##") + std::to_string((uint64_t) &asset)), &asset.previewScale, 0.1f, 2.0f, "%0.1f", 0);
                            UIText(CSTR(ELECTRON_GET_LOCALIZATION(instance, "ASSET_MANAGER_TEXTURE_RESOLUTION") + std::string(": ") + std::to_string((int) textureDimensions.x) + "x" + std::to_string((int) textureDimensions.y)));
                            break;
                        }
                    }

                    UIText(CSTR(ELECTRON_GET_LOCALIZATION(instance, "ASSET_MANAGER_ASSET_ID") + std::string(": ") + intToHex(asset.id)));
                    UISameLine();
                    if (UIButton(ELECTRON_GET_LOCALIZATION(instance, "GENERIC_COPY_TO_CLIPBOARD"))) {
                        ClipSetText(intToHex(asset.id));
                    }

                    std::string assetName = asset.name;
                    if (UIInputField(CSTR(ELECTRON_GET_LOCALIZATION(instance, "ASSET_MANAGER_RESOURCE_NAME") + std::string("##") + std::to_string((uint64_t) &asset)), &assetName, ImGuiInputTextFlags_EnterReturnsTrue)) {
                        asset.name = assetName;
                    }

                    std::string reservedResourcePath = asset.path;
                    std::string assetPath = asset.path;
                    if (UIButton(CSTR(std::string(ELECTRON_GET_LOCALIZATION(instance, "GENERIC_BROWSE")) + "##" + std::to_string((uint64_t) &asset)))) {
                        FileDialogImplOpenDialog("BrowseAssetPath", ELECTRON_GET_LOCALIZATION(instance, "ASSET_MANAGER_BROWSE_ASSET_PATH"), IMPORT_EXTENSIONS, ".");
                        targetAssetBrowsePath = assetIndex;
                    }
                    UISameLine();
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
                if (assets.at(assetDeletionIndex).type == TextureUnionType::Texture) {
                    PixelBufferImplDestroyGPUTexture(assets.at(assetDeletionIndex).pboGpuTexture);
                }
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

        if (FileDialogImplDisplay("BrowseAssetPath")) {
            if (FileDialogImplIsOK()) {
                AssetRegistry& reg = instance->assets;
                reg.assets[targetAssetBrowsePath].path = FileDialogImplGetFilePathName();
                TextureUnionImplRebuildAssetData(&reg.assets[targetAssetBrowsePath], &instance->graphics);
            }

            FileDialogImplClose();
        }
    }
}