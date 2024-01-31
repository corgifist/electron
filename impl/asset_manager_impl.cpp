#include "editor_core.h"
#include "app.h"
#include "ImGui/ImGuiFileDialog.h"
#include "utils/drag_utils.h"
#define CSTR(x) ((x).c_str())

using namespace Electron;

extern "C" {

    static void DrawRect(RectBounds bounds, ImVec4 color) {
        ImGui::GetWindowDrawList()->AddRectFilled(bounds.UL, bounds.BR, ImGui::ColorConvertFloat4ToU32(color));
    }


    ELECTRON_EXPORT void UIRender() {
        ImGui::SetCurrentContext(AppCore::context);

        UI::Begin(CSTR(std::string(ICON_FA_FOLDER " ") + ELECTRON_GET_LOCALIZATION("ASSET_MANAGER_TITLE") + std::string("##") + std::to_string(UICounters::AssetManagerCounter)), Signal::_CloseWindow, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
            ImVec2 windowSize = ImGui::GetContentRegionAvail();
            if (!AppCore::projectOpened) {
                std::string noProjectOpened = ELECTRON_GET_LOCALIZATION("GENERIC_NO_PROJECT_IS_OPENED");
                ImVec2 textSize = ImGui::CalcTextSize(CSTR(noProjectOpened));
                ImGui::SetCursorPos(ImVec2{windowSize.x / 2.0f - textSize.x / 2.0f, windowSize.y / 2.0f - textSize.y / 2.0f});
                ImGui::Text("%s", CSTR(noProjectOpened));
                ImGui::End();
                return;
            }

            static std::string searchFilter = JSON_AS_TYPE(Shared::project.propertiesMap["AssetSearch"], std::string);
            Shared::project.propertiesMap["AssetSearch"] = searchFilter;
            static float beginCursorY = 0;
            static float endCursorY = 0;

            if (Shared::assetSelected == -128) {
                Shared::assetSelected = JSON_AS_TYPE(Shared::project.propertiesMap["AssetSelected"], int);
            }
            Shared::project.propertiesMap["AssetSelected"] = Shared::assetSelected;

            bool p = false;

            ImGui::BeginChild("searchBarChild", ImVec2(windowSize.x, endCursorY - beginCursorY), false, 0);
            if (Shared::assetSelected >= AssetCore::assets.size()) Shared::assetSelected = -1;
                if (Shared::assetSelected == -1) {
                    std::string noAssetSelectedMsg = ELECTRON_GET_LOCALIZATION("ASSET_MANAGER_NO_ASSET_SELECTED");
                    ImGui::SetCursorPosX(ImGui::GetWindowSize().x / 2.0f - ImGui::CalcTextSize(CSTR(noAssetSelectedMsg)).x / 2.0f);
                    ImGui::Text("%s", CSTR(noAssetSelectedMsg));
                } else {
                    TextureUnion& asset = AssetCore::assets[Shared::assetSelected];
                    ImVec2 acceptedPreviewResolution = {ImGui::GetContentRegionAvail().x * 0.35f, 128};
                    GLuint gpuPreview = -1;
                    glm::vec2 assetResolution = {0, 0};
                    switch (asset.type) {
                        case TextureUnionType::Audio:
                        case TextureUnionType::Texture: {
                            gpuPreview = asset.pboGpuTexture;
                            assetResolution = asset.GetDimensions();
                            break;
                        }
                    }
                    glm::vec2 reservedResolution = assetResolution;
                    float xAspect = assetResolution.x / acceptedPreviewResolution.x;
                    assetResolution = {acceptedPreviewResolution.x, asset.GetDimensions().y / xAspect};
                    if (ImGui::BeginTable("infoTable", 2, ImGuiTableFlags_SizingFixedFit)) {
                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(0);
                        ImGui::Image((ImTextureID)(uint64_t) gpuPreview, ImVec2{assetResolution.x, assetResolution.y});
                        ImGui::TableSetColumnIndex(1);
                        ImGui::InputText("##AssetName", &asset.name, 0);
                        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) 
                            ImGui::SetTooltip("%s", CSTR(string_format("%s %s", ICON_FA_PENCIL, ELECTRON_GET_LOCALIZATION("ASSET_MANAGER_RESOURCE_NAME"))));
                        std::string hexId = intToHex(asset.id);
                        ImGui::Text("%s", string_format("%s: %s", ELECTRON_GET_LOCALIZATION("ASSET_MANAGER_ASSET_ID"), hexId.c_str()).c_str());
                        ImGui::SameLine();
                        if (ImGui::Button(CSTR(string_format("%s %s", ICON_FA_COPY, ELECTRON_GET_LOCALIZATION("GENERIC_COPY_ID"))))) {
                            ImGui::SetClipboardText(CSTR(hexId));
                        }
                        glm::vec2 naturalAssetReoslution = asset.GetDimensions();
                        ImGui::Text("%s %s", ICON_FA_ARROW_POINTER, ELECTRON_GET_LOCALIZATION("HOVER_TO_GET_PROBE_DATA"));
                        if (ImGui::IsItemHovered()) {
                            std::string probeData = asset.ffprobeData;
                            if (ImGui::GetIO().MouseClicked[ImGuiMouseButton_Left]) {
                                ImGui::SetClipboardText(probeData.c_str());
                            }
                            ImGui::SetTooltip("%s", probeData.c_str());
                        }
                        ImGui::EndTable();
                    }
                }
                ImGui::Spacing(); ImGui::Spacing();
                ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x);
                ImGui::InputText("##SearchFilterChild", &searchFilter, 0);
                if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                    ImGui::SetTooltip("%s", string_format("%s %s", ICON_FA_MAGNIFYING_GLASS, ELECTRON_GET_LOCALIZATION("ASSET_MANAGER_SEARCH")).c_str());
                if (ImGui::Selectable(string_format("%s %s", ICON_FA_FILE_IMPORT, ELECTRON_GET_LOCALIZATION("ASSET_MANAGER_IMPORT_ASSET")).c_str(), p)) {
                    ImGuiFileDialog::Instance()->OpenDialog("ImportAssetDlg", ELECTRON_GET_LOCALIZATION("ASSET_MANAGER_SELECT_ASSET_PATH"), IMPORT_EXTENSIONS, ".");
                }
                endCursorY = ImGui::GetCursorPos().y;
            ImGui::EndChild();
            ImGui::BeginChild("assetSelectables", ImVec2(windowSize.x, windowSize.y - (endCursorY - beginCursorY)), false, ImGuiWindowFlags_AlwaysVerticalScrollbar);
            int assetIndex = 0;
            int assetDeletionIndex = -1;
            static int targetAssetBrowsePath = -1;
            if (ImGui::BeginTable("AssetsTable", 3, ImGuiTableFlags_Resizable)) {
                ImGui::TableSetupColumn(ELECTRON_GET_LOCALIZATION("ASSET_MANAGER_NAME"));
                ImGui::TableSetupColumn(ELECTRON_GET_LOCALIZATION("ASSET_MANAGER_PATH"));
                ImGui::TableSetupColumn(ELECTRON_GET_LOCALIZATION("ASSET_MANAGER_SIZE"));
                ImGui::TableHeadersRow();
                for (auto& asset : AssetCore::assets) {
                    ImGui::TableNextRow();
                    if (searchFilter != "" && (asset.name.find(searchFilter) == std::string::npos || hexToInt(trim_copy(searchFilter)) != asset.id)) 
                        continue;
                    std::string assetIcon = asset.GetIcon();
                    if (!asset.ready) assetIcon = ICON_FA_SPINNER;
                    std::string reservedResourcePath = asset.path;
                    bool audioLoaded = false;
                    if (asset.type == TextureUnionType::Audio) {
                        auto audioStatusResponse =  Servers::AudioServerRequest({
                            {"action", "is_loaded"},
                            {"path", asset.path}
                        });
                        audioLoaded = JSON_AS_TYPE(
                            audioStatusResponse["loaded"], bool);
                        if (!audioLoaded) assetIcon = ICON_FA_SPINNER;
                        if (!audioLoaded) {
                            Servers::AudioServerRequest({
                                {"action", "load_sample"},
                                {"path", asset.path}
                            });
                        }
                    }
                    for (int column = 0; column < 3; column++) {
                        ImGui::TableSetColumnIndex(column);
                        if (column == 0) {
                            if (ImGui::Selectable(CSTR(string_format("%s %s##%i", CSTR(assetIcon), CSTR(asset.ready ? asset.name : ELECTRON_GET_LOCALIZATION("GENERIC_IMPORTING")), asset.id)), p, ImGuiSelectableFlags_SpanAllColumns)) {
                                if (asset.ready) {
                                    if (asset.type == TextureUnionType::Audio && audioLoaded)
                                        Shared::assetSelected = assetIndex;
                                    else if (asset.type != TextureUnionType::Audio) {
                                        Shared::assetSelected = assetIndex;
                                    }
                                }
                            }

                            if (ImGui::IsItemHovered(0) && ImGui::GetIO().MouseDown[ImGuiMouseButton_Right]) {
                                ImGui::OpenPopup(string_format("AssetPopup%i", asset.id).c_str(), 0);
                            }

                            if (ImGui::BeginPopup(string_format("AssetPopup%i", asset.id).c_str(), 0)) {
                                ImGui::SeparatorText(CSTR(asset.name));
                                if (ImGui::MenuItem(CSTR(string_format("%s %s", ICON_FA_MAGNIFYING_GLASS, ELECTRON_GET_LOCALIZATION("ASSET_MANAGER_EXAMINE_ASSET"))), nullptr, false, UICounters::AssetExaminerCounter != 1)) {
                                    AppCore::ExecuteSignal(Signal::_SpawnAssetExaminer);
                                }
                                if (ImGui::MenuItem(CSTR(string_format("%s %s", ICON_FA_RECYCLE, ELECTRON_GET_LOCALIZATION("ASSET_MANAGER_REIMPORT_ASSET"))), "")) {
                                    ImGuiFileDialog::Instance()->OpenDialog("BrowseAssetPath", ELECTRON_GET_LOCALIZATION("ASSET_MANAGER_BROWSE_ASSET_PATH"), IMPORT_EXTENSIONS, ".");
                                    targetAssetBrowsePath = assetIndex;
                                }
                                if (ImGui::MenuItem(string_format("%s %s", ICON_FA_TRASH_CAN, ELECTRON_GET_LOCALIZATION("GENERIC_DELETE")).c_str(), "")) {
                                    assetDeletionIndex = assetIndex;
                                    if (assetIndex == Shared::assetSelected) {
                                        Shared::assetSelected = -1;
                                    }
                                }
                                ImGui::EndPopup();
                            }
                        }
                        if (column == 1) {
                            std::string path = asset.path;
                            path = std::filesystem::relative(path).string();
                            ImGui::Text("%s", CSTR(path));
                        }
                        if (column == 2) {
                            ImGui::Text("%0.1f MB", filesize(asset.path.c_str()) / 1024.0f / 1024.0f);
                        }
                    }
                
                    assetIndex++;
            }
            ImGui::EndTable();
            }
            ImGui::EndChild();
        UI::End();



        
        if (ImGuiFileDialog::Instance()->Display("ImportAssetDlg")) {
            if (ImGuiFileDialog::Instance()->IsOk()) {
                Shared::importErrorMessage = AssetCore::ImportAsset(ImGuiFileDialog::Instance()->GetFilePathName());
            }
            ImGuiFileDialog::Instance()->Close();
        }

        if (ImGuiFileDialog::Instance()->Display("BrowseAssetPath")) {
            if (ImGuiFileDialog::Instance()->IsOk()) {
                AssetCore::assets[targetAssetBrowsePath].path = ImGuiFileDialog::Instance()->GetFilePathName();
                AssetCore::assets[targetAssetBrowsePath].RebuildAssetData();
            }

            ImGuiFileDialog::Instance()->Close();
        }

        if (Shared::importErrorMessage != "") {
            std::string popupTitle = string_format("%s %s", ICON_FA_EXCLAMATION, ELECTRON_GET_LOCALIZATION("ASSET_MANAGER_IMPORT_FAILURE"));
            ImGui::OpenPopup(popupTitle.c_str());
            if (ImGui::BeginPopupModal(popupTitle.c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoResize)) {
                ImGui::SetCursorPosX(ImGui::GetWindowSize().x / 2.0f - ImGui::CalcTextSize(CSTR(Shared::importErrorMessage)).x / 2.0f);
                ImGui::Text("%s", CSTR(Shared::importErrorMessage));

                if (AppCore::ButtonCenteredOnLine(ELECTRON_GET_LOCALIZATION("GENERIC_OK"))) {
                    Shared::importErrorMessage = "";
                }
                ImGui::EndPopup();
            }
            
        }

        if (assetDeletionIndex != -1) {
            auto& assets = AssetCore::assets;
            TextureUnion& asset = assets.at(assetDeletionIndex);
            TextureUnionType type = assets.at(assetDeletionIndex).type;
            if (type == TextureUnionType::Texture || type == TextureUnionType::Audio) {
                PixelBuffer::DestroyGPUTexture(assets.at(assetDeletionIndex).pboGpuTexture);
            }
            for (auto& path : asset.linkedCache) {
                std::filesystem::remove(path);
            }
            assets.erase(assets.begin() + assetDeletionIndex);
        }
    }
}