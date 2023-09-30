#include "editor_core.h"
#include "app.h"
#include "ImGuiFileDialog.h"
#define CSTR(x) ((x).c_str())

using namespace Electron;

extern "C" {
    ELECTRON_EXPORT void AssetManagerRender(AppInstance* instance) {
        ImGui::SetCurrentContext(instance->context);
        UI::Begin(CSTR(std::string(ICON_FA_FOLDER " ") + ELECTRON_GET_LOCALIZATION(instance, "ASSET_MANAGER_TITLE") + std::string("##") + std::to_string(UICounters::AssetManagerCounter)), ElectronSignal_CloseWindow, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
            ImVec2 windowSize = ImGui::GetContentRegionAvail();
            if (!instance->projectOpened) {
                std::string noProjectOpened = ELECTRON_GET_LOCALIZATION(instance, "GENERIC_NO_PROJECT_IS_OPENED");
                ImVec2 textSize = ImGui::CalcTextSize(CSTR(noProjectOpened));
                ImGui::SetCursorPos(ImVec2{windowSize.x / 2.0f - textSize.x / 2.0f, windowSize.y / 2.0f - textSize.y / 2.0f});
                ImGui::Text(CSTR(noProjectOpened));
                ImGui::End();
                return;
            }

            static std::string importErrorMessage = "";
            static std::string searchFilter = JSON_AS_TYPE(instance->project.propertiesMap["AssetSearch"], std::string);
            instance->project.propertiesMap["AssetSearch"] = searchFilter;
            static float beginCursorY = 0;
            static float endCursorY = 0;

            static int assetSelected = JSON_AS_TYPE(instance->project.propertiesMap["AssetSelected"], int);
            instance->project.propertiesMap["AssetSelected"] = assetSelected;

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

            ImGui::BeginChild("searchBarChild", ImVec2(windowSize.x, endCursorY - beginCursorY), false, 0);
                if (assetSelected == -1) {
                    std::string noAssetSelectedMsg = ELECTRON_GET_LOCALIZATION(instance, "ASSET_MANAGER_NO_ASSET_SELECTED");
                    ImGui::SetCursorPosX(ImGui::GetWindowSize().x / 2.0f - ImGui::CalcTextSize(CSTR(noAssetSelectedMsg)).x / 2.0f);
                    ImGui::Text(CSTR(noAssetSelectedMsg));
                } else {
                    TextureUnion& asset = instance->assets.assets[assetSelected];
                    ImVec2 acceptedPreviewResolution = {ImGui::GetContentRegionAvail().x * 0.35f, 128};
                    GLuint gpuPreview = -1;
                    glm::vec2 assetResolution = {0, 0};
                    switch (asset.type) {
                        case TextureUnionType::Texture: {
                            gpuPreview = asset.pboGpuTexture;
                            assetResolution = asset.GetDimensions();
                            break;
                        }
                    }
                    glm::vec2 reservedResolution = assetResolution;
                    float xAspect = precision((float) assetResolution.x / (float) acceptedPreviewResolution.x, 2);
                    assetResolution = {acceptedPreviewResolution.x, (int) (asset.GetDimensions().y / precision(xAspect, 2))};
                    ImGui::BeginTable("infoTable", 2, ImGuiTableFlags_SizingFixedFit);
                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(0);
                        ImGui::Image((ImTextureID) gpuPreview, ImVec2{assetResolution.x, assetResolution.y});
                        ImGui::TableSetColumnIndex(1);
                        ImGui::InputText("##AssetName", &asset.name, 0);
                        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) 
                            ImGui::SetTooltip(CSTR(string_format("%s %s", ICON_FA_PENCIL, ELECTRON_GET_LOCALIZATION(instance, "ASSET_MANAGER_RESOURCE_NAME"))));
                        std::string hexId = intToHex(asset.id);
                        ImGui::Text(string_format("%s: %s", ELECTRON_GET_LOCALIZATION(instance, "ASSET_MANAGER_ASSET_ID"), hexId.c_str()).c_str());
                        ImGui::SameLine();
                        if (ImGui::Button(CSTR(string_format("%s %s", ICON_FA_COPY, ELECTRON_GET_LOCALIZATION(instance, "GENERIC_COPY_ID"))))) {
                            clip::set_text(hexId);
                        }
                        glm::vec2 naturalAssetReoslution = asset.GetDimensions();
                        ImGui::Text(CSTR(string_format("%s: %ix%i", ELECTRON_GET_LOCALIZATION(instance, "ASSET_MANAGER_TEXTURE_RESOLUTION"), (int) naturalAssetReoslution.x, (int) naturalAssetReoslution.y)));
                    ImGui::EndTable();
                }
                ImGui::Spacing(); ImGui::Spacing();
                ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x);
                ImGui::InputText("##SearchFilterChild", &searchFilter, 0);
                if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                    ImGui::SetTooltip(string_format("%s %s", ICON_FA_MAGNIFYING_GLASS, ELECTRON_GET_LOCALIZATION(instance, "ASSET_MANAGER_SEARCH")).c_str());
                if (ImGui::Selectable(string_format("%s %s", ICON_FA_FILE_IMPORT, ELECTRON_GET_LOCALIZATION(instance, "ASSET_MANAGER_IMPORT_ASSET")).c_str(), p)) {
                    ImGuiFileDialog::Instance()->OpenDialog("ImportAssetDlg", ELECTRON_GET_LOCALIZATION(instance, "ASSET_MANAGER_SELECT_ASSET_PATH"), IMPORT_EXTENSIONS, ".");
                }
                endCursorY = ImGui::GetCursorPos().y;
            ImGui::EndChild();
            ImGui::BeginChild("assetSelectables", ImVec2(windowSize.x, windowSize.y - (endCursorY - beginCursorY)), false, ImGuiWindowFlags_AlwaysVerticalScrollbar);
            int assetIndex = 0;
            int assetDeletionIndex = -1;
            static int targetAssetBrowsePath = -1;
            if (ImGui::BeginTable("AssetsTable", 3, ImGuiTableFlags_Resizable)) {
                ImGui::TableSetupColumn(ELECTRON_GET_LOCALIZATION(instance, "ASSET_MANAGER_NAME"));
                ImGui::TableSetupColumn(ELECTRON_GET_LOCALIZATION(instance, "ASSET_MANAGER_PATH"));
                ImGui::TableSetupColumn(ELECTRON_GET_LOCALIZATION(instance, "ASSET_MANAGER_SIZE"));
                ImGui::TableHeadersRow();
                for (auto& asset : instance->assets.assets) {
                    ImGui::TableNextRow();
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
                        ImGui::TableSetColumnIndex(column);
                        if (column == 0) {
                            if (ImGui::Selectable(CSTR(string_format("%s %s", CSTR(assetIcon), CSTR(asset.name))), p, ImGuiSelectableFlags_SpanAllColumns)) {
                                assetSelected = assetIndex;
                            }

                            if (ImGui::IsItemHovered(0) && ImGui::GetIO().MouseDown[ImGuiMouseButton_Right]) {
                                ImGui::OpenPopup(string_format("AssetPopup%i", asset.id).c_str(), 0);
                            }

                            if (ImGui::BeginPopup(string_format("AssetPopup%i", asset.id).c_str(), 0)) {
                                ImGui::SeparatorText(CSTR(asset.name));
                                if (ImGui::MenuItem(CSTR(string_format("%s %s", ICON_FA_RECYCLE, ELECTRON_GET_LOCALIZATION(instance, "ASSET_MANAGER_REIMPORT_ASSET"))), "")) {
                                    ImGuiFileDialog::Instance()->OpenDialog("BrowseAssetPath", ELECTRON_GET_LOCALIZATION(instance, "ASSET_MANAGER_BROWSE_ASSET_PATH"), IMPORT_EXTENSIONS, ".");
                                    targetAssetBrowsePath = assetIndex;
                                }
                                if (ImGui::MenuItem(string_format("%s %s", ICON_FA_TRASH_CAN, ELECTRON_GET_LOCALIZATION(instance, "GENERIC_DELETE")).c_str(), "")) {
                                    assetDeletionIndex = assetIndex;
                                    if (assetIndex == assetSelected) {
                                        assetSelected = -1;
                                    }
                                }
                                ImGui::EndPopup();
                            }
                        }
                        if (column == 1) {
                            std::string path = asset.path;
                            path = std::filesystem::relative(path).string();
                            ImGui::Text(CSTR(path));
                        }
                        if (column == 2) {
                            ImGui::Text(CSTR(std::to_string((float) filesize(asset.path.c_str()) / 1024.0f / 1024.0f)));
                        }
                    }
                
                    assetIndex++;
            }
            ImGui::EndTable();
            }
            ImGui::EndChild();
        ImGui::End();

        
        if (ImGuiFileDialog::Instance()->Display("ImportAssetDlg")) {
            if (ImGuiFileDialog::Instance()->IsOk()) {
                importErrorMessage = instance->assets.ImportAsset(ImGuiFileDialog::Instance()->GetFilePathName());
            }
            ImGuiFileDialog::Instance()->Close();
        }

        if (ImGuiFileDialog::Instance()->Display("BrowseAssetPath")) {
            if (ImGuiFileDialog::Instance()->IsOk()) {
                AssetRegistry& reg = instance->assets;
                reg.assets[targetAssetBrowsePath].path = ImGuiFileDialog::Instance()->GetFilePathName();
                reg.assets[targetAssetBrowsePath].RebuildAssetData(&instance->graphics);
            }

            ImGuiFileDialog::Instance()->Close();
        }

        if (importErrorMessage != "") {
            UI::Begin(CSTR(string_format("%s %s", ICON_FA_EXCLAMATION, ELECTRON_GET_LOCALIZATION(instance, "ASSET_MANAGER_IMPORT_FAILURE"))), ElectronSignal_None, ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize);
                ImGui::SetCursorPosX(ImGui::GetWindowSize().x / 2.0f - ImGui::CalcTextSize(CSTR(importErrorMessage)).x / 2.0f);
                ImGui::Text(CSTR(importErrorMessage));

                if (instance->ButtonCenteredOnLine(ELECTRON_GET_LOCALIZATION(instance, "GENERIC_OK"))) {
                    importErrorMessage = "";
                }
            UI::End();
        }

        if (assetDeletionIndex != -1) {
            auto& assets = instance->assets.assets;
            if (assets.at(assetDeletionIndex).type == TextureUnionType::Texture) {
                PixelBuffer::DestroyGPUTexture(assets.at(assetDeletionIndex).pboGpuTexture);
            }
            assets.erase(assets.begin() + assetDeletionIndex);
        }
    }
}