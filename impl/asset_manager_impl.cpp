#include "editor_core.h"
#include "ui_api.h"

using namespace Electron;

extern "C" {
    ELECTRON_EXPORT void AssetManagerRender(AppInstance* instance) {
        UIBegin(CSTR(ELECTRON_GET_LOCALIZATION(instance, "ASSET_MANAGER_TITLE") + std::string("##") + std::to_string(CounterGetAssetManager())), ElectronSignal_CloseWindow, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
            ImVec2 windowSize = UIGetContentRegionAvail();
            if (!instance->projectOpened) {
                std::string noProjectOpened = ELECTRON_GET_LOCALIZATION(instance, "GENERIC_NO_PROJECT_IS_OPENED");
                ImVec2 textSize = UICalcTextSize(CSTR(noProjectOpened));
                UISetCursorPos(ImVec2{windowSize.x / 2.0f - textSize.x / 2.0f, windowSize.y / 2.0f - textSize.y / 2.0f});
                UIText(CSTR(noProjectOpened));
                UIEnd();
                return;
            }

            static std::string importErrorMessage = "";
            static std::string searchFilter = "";
            static float beginCursorY = 0;
            static float endCursorY = 0;

            static int assetSelected = -1;
            if (instance->graphics.fireAssetId != -1) {
                int iterIndex = 0;
                for (auto& asset : instance->assets.assets) {
                    if (asset.id == instance->graphics.fireAssetId) {
                        assetSelected = iterIndex;
                        break;
                    }
                    iterIndex++;
                }
                instance->graphics.fireAssetId = -1;
            }
            bool p = false;

            UIBeginChild("searchBarChild", ImVec2(windowSize.x, endCursorY - beginCursorY), false, 0);
                if (assetSelected == -1) {
                    std::string noAssetSelectedMsg = ELECTRON_GET_LOCALIZATION(instance, "ASSET_MANAGER_NO_ASSET_SELECTED");
                    UISetCursorX(ImGui::GetWindowSize().x / 2.0f - UICalcTextSize(CSTR(noAssetSelectedMsg)).x / 2.0f);
                    UIText(CSTR(noAssetSelectedMsg));
                } else {
                    TextureUnion& asset = instance->assets.assets[assetSelected];
                    ImVec2 acceptedPreviewResolution = {UIGetContentRegionAvail().x * 0.35f, 128};
                    GLuint gpuPreview = -1;
                    glm::vec2 assetResolution = {0, 0};
                    switch (asset.type) {
                        case TextureUnionType::Texture: {
                            gpuPreview = asset.pboGpuTexture;
                            assetResolution = TextureUnionImplGetDimensions(&asset);
                            break;
                        }
                    }
                    glm::vec2 reservedResolution = assetResolution;
                    float xAspect = precision((float) assetResolution.x / (float) acceptedPreviewResolution.x, 2);
                    assetResolution = {acceptedPreviewResolution.x, TextureUnionImplGetDimensions(&asset).y / xAspect};
                    UIBeginTable("infoTable", 2, ImGuiTableFlags_SizingFixedFit);
                        UITableNextRow();
                        UITableSetColumnIndex(0);
                        UIImage(gpuPreview, ImVec2{assetResolution.x, assetResolution.y});
                        UITableSetColumnIndex(1);
                        UIInputField("##AssetName", &asset.name, 0);
                        if (UIIsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) 
                            UISetTooltip(CSTR(string_format("%s %s", ICON_FA_PENCIL, ELECTRON_GET_LOCALIZATION(instance, "ASSET_MANAGER_RESOURCE_NAME"))));
                        std::string hexId = intToHex(asset.id);
                        UIText(string_format("%s: %s", ELECTRON_GET_LOCALIZATION(instance, "ASSET_MANAGER_ASSET_ID"), hexId.c_str()).c_str());
                        UISameLine();
                        if (UIButton(CSTR(string_format("%s %s", ICON_FA_COPY, ELECTRON_GET_LOCALIZATION(instance, "GENERIC_COPY_ID"))))) {
                            ClipSetText(hexId);
                        }
                        glm::vec2 naturalAssetReoslution = TextureUnionImplGetDimensions(&asset);
                        UIText(CSTR(string_format("%s: %ix%i", ELECTRON_GET_LOCALIZATION(instance, "ASSET_MANAGER_TEXTURE_RESOLUTION"), (int) naturalAssetReoslution.x, (int) naturalAssetReoslution.y)));
                    UIEndTable();
                }
                UISpacing(); UISpacing();
                UIPushItemWidth(UIGetContentRegionAvail().x);
                UIInputField("##SearchFilterChild", &searchFilter, 0);
                UISetItemTooltip(string_format("%s %s", ICON_FA_MAGNIFYING_GLASS, ELECTRON_GET_LOCALIZATION(instance, "ASSET_MANAGER_SEARCH")).c_str());
                if (UISelectable(string_format("%s %s", ICON_FA_FILE_IMPORT, ELECTRON_GET_LOCALIZATION(instance, "ASSET_MANAGER_IMPORT_ASSET")).c_str(), p)) {
                    FileDialogImplOpenDialog("ImportAssetDlg", ELECTRON_GET_LOCALIZATION(instance, "ASSET_MANAGER_SELECT_ASSET_PATH"), IMPORT_EXTENSIONS, ".");
                }
                endCursorY = UIGetCursorPos().y;
            UIEndChild();
            UIBeginChild("assetSelectables", ImVec2(windowSize.x, windowSize.y - (endCursorY - beginCursorY)), false, ImGuiWindowFlags_AlwaysVerticalScrollbar);
            int assetIndex = 0;
            int assetDeletionIndex = -1;
            static int targetAssetBrowsePath = -1;
            if (UIBeginTable("AssetsTable", 3, ImGuiTableFlags_Resizable)) {
                UITableSetupColumn(ELECTRON_GET_LOCALIZATION(instance, "ASSET_MANAGER_NAME"));
                UITableSetupColumn(ELECTRON_GET_LOCALIZATION(instance, "ASSET_MANAGER_PATH"));
                UITableSetupColumn(ELECTRON_GET_LOCALIZATION(instance, "ASSET_MANAGER_SIZE"));
                UITableUploadHeaders();
                for (auto& asset : instance->assets.assets) {
                    UITableNextRow();
                    if (searchFilter != "" && asset.name.find(searchFilter)== std::string::npos) 
                        continue;
                    std::string assetIcon = ICON_FA_IMAGE;
                    switch (asset.type) {
                        case TextureUnionType::Texture: {
                            assetIcon = ICON_FA_IMAGE;
                            break;
                        }
                    }
                    std::string reservedResourcePath = asset.path;
                    for (int column = 0; column < 3; column++) {
                        UITableSetColumnIndex(column);
                        if (column == 0) {
                            if (UISelectableFlag(CSTR(string_format("%s %s", CSTR(assetIcon), CSTR(asset.name))), p, ImGuiSelectableFlags_SpanAllColumns)) {
                                assetSelected = assetIndex;
                            }

                            if (UIIsItemHovered(0) && UIGetIO().MouseDown[ImGuiMouseButton_Right]) {
                                UIOpenPopup(string_format("AssetPopup%i", asset.id), 0);
                            }

                            if (UIBeginPopup(string_format("AssetPopup%i", asset.id), 0)) {
                                UISeparatorText(CSTR(asset.name));
                                if (UIMenuItem(CSTR(string_format("%s %s", ICON_FA_RECYCLE, ELECTRON_GET_LOCALIZATION(instance, "ASSET_MANAGER_REIMPORT_ASSET"))), "")) {
                                    FileDialogImplOpenDialog("BrowseAssetPath", ELECTRON_GET_LOCALIZATION(instance, "ASSET_MANAGER_BROWSE_ASSET_PATH"), IMPORT_EXTENSIONS, ".");
                                    targetAssetBrowsePath = assetIndex;
                                }
                                if (UIMenuItem(string_format("%s %s", ICON_FA_TRASH_CAN, ELECTRON_GET_LOCALIZATION(instance, "GENERIC_DELETE")).c_str(), "")) {
                                    assetDeletionIndex = assetIndex;
                                    if (assetIndex == assetSelected) {
                                        assetSelected = -1;
                                    }
                                }
                                UIEndPopup();
                            }
                        }
                        if (column == 1) {
                            std::string path = asset.path;
                            path = std::filesystem::relative(path).string();
                            UIText(CSTR(path));
                        }
                        if (column == 2) {
                            UIText(CSTR(std::to_string((float) filesize(asset.path.c_str()) / 1024.0f / 1024.0f)));
                        }
                    }
                
                    assetIndex++;
            }
            UIEndTable();
            }
            UIEndChild();
        UIEnd();

        
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

        if (importErrorMessage != "") {
            UIBegin(CSTR(string_format("%s %s", ICON_FA_EXCLAMATION, ELECTRON_GET_LOCALIZATION(instance, "ASSET_MANAGER_IMPORT_FAILURE"))), ElectronSignal_None, ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize);
                UISetCursorX(UIGetWindowSize().x / 2.0f - UICalcTextSize(CSTR(importErrorMessage)).x / 2.0f);
                UIText(CSTR(importErrorMessage));

                if (UICenteredButton(instance, ELECTRON_GET_LOCALIZATION(instance, "GENERIC_OK"))) {
                    importErrorMessage = "";
                }
            UIEnd();
        }

        if (assetDeletionIndex != -1) {
            auto& assets = instance->assets.assets;
            if (assets.at(assetDeletionIndex).type == TextureUnionType::Texture) {
                PixelBufferImplDestroyGPUTexture(assets.at(assetDeletionIndex).pboGpuTexture);
            }
            assets.erase(assets.begin() + assetDeletionIndex);
        }
    }
}