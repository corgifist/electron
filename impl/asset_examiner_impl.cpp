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

        static AssetDecoder assetDecoder;
        static int previousAssetID = -1;

        if (Shared::assetSelected != -1 && Shared::assetSelected != -128 && AppCore::projectOpened && Shared::assetSelected < AssetCore::assets.size()) {
            UI::Begin(CSTR(string_format("%s %s", ICON_FA_MAGNIFYING_GLASS, ELECTRON_GET_LOCALIZATION("ASSET_MANAGER_ASSET_EXAMINER"))), Signal::_CloseWindow, ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoScrollbar);
                ImVec2 windowSize = ImGui::GetContentRegionAvail();
                static float assetPreviewScale = JSON_AS_TYPE(Shared::project.propertiesMap["LastAssetPreviewScale"], float);
                Shared::project.propertiesMap["LastAssetPreviewScale"] = assetPreviewScale;
                if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_KeypadAdd)) {
                    assetPreviewScale += 0.05f;
                }
                if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_KeypadSubtract)) {
                    assetPreviewScale -= 0.05f;
                }

                if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows) && ImGui::GetIO().MouseWheel != 0.0f) 
                    assetPreviewScale += (float) ImGui::GetIO().MouseWheel / 5.0f;
                static float propertiesHeight = 20;
                ImGui::BeginChild("previewExamineChild", ImVec2(windowSize.x, windowSize.y - propertiesHeight), 0, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
                windowSize = ImGui::GetContentRegionAvail();
                TextureUnion& asset = AssetCore::assets.at(Shared::assetSelected);
                if (previousAssetID != asset.id) {
                    if (assetDecoder.id) assetDecoder.Destroy();
                    assetDecoder = AssetDecoder();
                }
                switch (asset.type) {
                    case TextureUnionType::Audio:
                    case TextureUnionType::Texture:
                    case TextureUnionType::Video: {
                        static DragStructure imageDrag{};
                        static ImVec2 imageOffset{};
                        ImVec2 dimension = {asset.GetDimensions().x, asset.GetDimensions().y};
                        ImVec2 imageSize = FitRectInRect(windowSize, dimension) * assetPreviewScale;
                        imageDrag.Activate(); float f;
                        if (imageDrag.GetDragDistance(f)) {
                            imageOffset += ImGui::GetIO().MouseDelta;
                            ImGuiStyle& style = ImGui::GetStyle();
                            DrawRect(RectBounds(ImVec2(windowSize.x / 2.0f + imageOffset.x - 2, 0), ImVec2(4.0f, windowSize.y)), style.Colors[ImGuiCol_MenuBarBg]);
                            DrawRect(RectBounds(ImVec2(0, windowSize.y / 2.0f + imageOffset.y - 2), ImVec2(windowSize.x, 4.0f)), style.Colors[ImGuiCol_MenuBarBg]);
                            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
                        } else imageDrag.Deactivate();
                        if (glm::abs(imageOffset.x) < 5) imageOffset.x = 0;
                        if (glm::abs(imageOffset.y) < 5) imageOffset.y = 0;

                        ImGui::SetCursorPos(ImVec2{windowSize.x / 2.0f - imageSize.x / 2.0f, windowSize.y / 2.0f - imageSize.y / 2.0f} + imageOffset);
                        if (!assetDecoder.AreHandlesLoaded()) assetDecoder.LoadHandle(&asset);
                        if (assetDecoder.AreHandlesLoaded()) {
                            ImGui::Image((ImTextureID) assetDecoder.GetImageHandle(&asset), imageSize);
                        }
                        if (ImGui::IsItemHovered() && ImGui::GetIO().MouseDown[ImGuiMouseButton_Right]) {
                            ImGui::OpenPopup("assetContextMenu");
                        }
                        if (asset.type == TextureUnionType::Audio || asset.type == TextureUnionType::Video) {
                            std::string quickInfoText = "";
                            if (asset.type == TextureUnionType::Audio) {
                                AudioMetadata audio = std::get<AudioMetadata>(asset.as);
                                quickInfoText = ICON_FA_AUDIO_DESCRIPTION + std::string(" ") + audio.codecName;
                                quickInfoText += " | " + std::to_string(audio.sampleRate) + " " + "Hz";
                            } else if (asset.type == TextureUnionType::Video) {
                                VideoMetadata video = std::get<VideoMetadata>(asset.as);
                                quickInfoText = ICON_FA_VIDEO + std::string(" ") + video.codecName;
                                quickInfoText += " | " + std::to_string(video.width) + "x" + std::to_string(video.height);
                            }
                            ImVec2 quickInfoSize = ImGui::CalcTextSize(quickInfoText.c_str());
                            ImGui::SetCursorPos({windowSize.x / 2.0f - quickInfoSize.x / 2.0f, 0});
                            ImGui::Text(quickInfoText.c_str());
                        }
                        if (ImGui::IsItemHovered()) {
                            ImGui::SetTooltip("%s", CSTR(string_format("%s %s", ICON_FA_ARROW_POINTER, ELECTRON_GET_LOCALIZATION("ASSET_MANAGER_LEFT_CLICK_FOR_CONTEXT_MENU"))));
                        }

                        break;
                    }
                }
                float seekRelativePos = 0;
                bool seekRequested = false;
                if (ImGui::BeginPopup("assetContextMenu")) {
                    ImGui::SeparatorText(asset.name.c_str());
                    if (asset.type == TextureUnionType::Video || asset.type == TextureUnionType::Audio) {
                        if (ImGui::BeginMenu(CSTR(string_format("%s %s", ICON_FA_TIMELINE, ELECTRON_GET_LOCALIZATION("SEEK"))))) {
                            bool emitSeconds = false;
                            bool seekingForward = false;
                            if (ImGui::BeginMenu(CSTR(string_format("%s %s", ICON_FA_FORWARD, ELECTRON_GET_LOCALIZATION("FORWARD"))))) {
                                for (int i = 1; i < 6; i++) {
                                    if (ImGui::MenuItem(CSTR(string_format("%s %is", ICON_FA_FORWARD, i * 2)))) {
                                        seekRelativePos = i * 2;
                                        seekRequested = true;
                                    }
                                }
                                ImGui::EndMenu();
                            }
                            if (ImGui::BeginMenu(CSTR(string_format("%s %s", ICON_FA_BACKWARD, ELECTRON_GET_LOCALIZATION("BACKWARD"))))) {
                                for (int i = 1; i < 6; i++) {
                                    if (ImGui::MenuItem(CSTR(string_format("%s %is", ICON_FA_BACKWARD, i * 2)))) {
                                        seekRelativePos = -i * 2;
                                        seekRequested = true;
                                    }
                                }
                                ImGui::EndMenu();
                            }
                            ImGui::EndMenu();
                        }
                    }
                    if (ImGui::MenuItem(CSTR(string_format("%s %s", ICON_FA_INFO, ELECTRON_GET_LOCALIZATION("ASSET_MANAGER_MORE_INFO"))))) {
                        if (system(CSTR(string_format("xdg-open %s &", asset.path.c_str())))) {}
                    }
                    if (asset.type == TextureUnionType::Audio && asset.audioCacheCover != "") {
                        if (ImGui::MenuItem(CSTR(string_format("%s %s", ICON_FA_FLOPPY_DISK, ELECTRON_GET_LOCALIZATION("IMPORT_COVER_AS_IMAGE"))))) {
                            Shared::importErrorMessage = AssetCore::ImportAsset(asset.audioCacheCover).returnMessage;
                        }
                    }
                    ImGui::EndPopup();
                }
                ImGui::EndChild();
                windowSize = ImGui::GetContentRegionAvail();
                ImGui::BeginChild("assetExaminerDetails", ImVec2(windowSize.x, 30), false);
                windowSize = ImGui::GetContentRegionAvail();
                if (ImGui::BeginTable("detailsTable", 2)) {
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    std::string assetIcon = asset.GetIcon();
                    std::string assetName = asset.name;
                    ImGui::Text("%s %s", assetIcon.c_str(), assetName.c_str());
                    ImGui::TableSetColumnIndex(1);
                    static float audioPlaybackProgress = 0;
                    static float audioPlaybackLength = 0;
                    static bool audioPlaybackPlaying = false;
                    static int audioHandle = 0;
                    static bool audioLoaded = false;
                    static bool audioHandleInitialized = false;
                    static bool usableAsAudio = false;
                    if (asset.type == TextureUnionType::Audio) usableAsAudio = true;
                    else if (asset.type == TextureUnionType::Video) usableAsAudio = AssetCore::GetAsset(intToHex(asset.linkedAudioAsset)) != nullptr;
                    else usableAsAudio = false;

                    if (previousAssetID != asset.id) {
                        Servers::AudioServerRequest({
                            {"action", "stop_sample"},
                            {"handle", audioHandle}
                        });
                    }

                    if (previousAssetID != asset.id && (asset.type == TextureUnionType::Audio || asset.type == TextureUnionType::Video)) {
                        float audioLength = asset.type == TextureUnionType::Audio ? std::get<AudioMetadata>(asset.as).audioLength : std::get<VideoMetadata>(asset.as).duration;
                        audioPlaybackPlaying = false;
                        audioHandleInitialized = false;
                        audioPlaybackLength = audioLength;
                        audioPlaybackProgress = 0;
                        if (usableAsAudio) {
                            std::string audioPath = asset.type == TextureUnionType::Audio ? asset.path : AssetCore::GetAsset(intToHex(asset.linkedAudioAsset))->path;
                            audioLoaded = JSON_AS_TYPE(
                            Servers::AudioServerRequest({
                                {"action", "is_loaded"},
                                {"path", audioPath}
                            })["loaded"], bool);
                            if (!audioLoaded) {
                                Servers::AudioServerRequest({
                                    {"action", "load_sample"},
                                    {"path", audioPath}
                                });
                            }
                        }
                    }
                    if (!audioHandleInitialized && audioLoaded && usableAsAudio) {
                        std::string audioPath = asset.type == TextureUnionType::Audio ? asset.path : AssetCore::GetAsset(intToHex(asset.linkedAudioAsset))->path;
                        if (audioHandle != 0) {
                            Servers::AudioServerRequest({
                                {"action", "stop_sample"},
                                {"handle", audioHandle}
                            });
                        }
                        auto audioResponse = Servers::AudioServerRequest({
                            {"action", "play_sample"},
                            {"path", audioPath}
                        });
                        audioHandle = JSON_AS_TYPE(audioResponse["handle"], int);
                        audioHandleInitialized = true;
                        Servers::AudioServerRequest({
                            {"action", "pause_sample"},
                            {"handle", audioHandle},
                            {"pause", !audioPlaybackPlaying}
                        });
                    }
                    switch (asset.type) {
                        case TextureUnionType::Texture: {
                            ImGui::Text("%s", CSTR(string_format("%s: %ix%i", ELECTRON_GET_LOCALIZATION("GENERIC_RESOLUTION"), (int) asset.GetDimensions().x, (int) asset.GetDimensions().y)));
                            break;
                        }
                        case TextureUnionType::Video:
                        case TextureUnionType::Audio: {
                            if (usableAsAudio) {
                                std::string audioPath = asset.type == TextureUnionType::Audio ? asset.path : AssetCore::GetAsset(intToHex(asset.linkedAudioAsset))->path;
                                audioLoaded = JSON_AS_TYPE(
                                Servers::AudioServerRequest({
                                    {"action", "is_loaded"},
                                    {"path", audioPath}
                                })["loaded"], bool);
                            } else {
                                audioLoaded = true;
                            }
                            audioPlaybackProgress = glm::clamp(audioPlaybackProgress, 0.0f, audioPlaybackLength);
                            if (audioPlaybackProgress >= audioPlaybackLength) audioPlaybackPlaying = false;
                            if (audioPlaybackPlaying) audioPlaybackProgress += Shared::deltaTime;
                            if (audioLoaded && ImGui::Button(audioPlaybackPlaying ? ICON_FA_SQUARE : ICON_FA_PLAY)) {
                                audioPlaybackPlaying = !audioPlaybackPlaying;
                            }
                            if (audioLoaded) {
                                Servers::AudioServerRequest({
                                    {"action", "pause_sample"},
                                    {"handle", audioHandle},
                                    {"pause", !audioPlaybackPlaying}
                                });
                            }

                            if (seekRequested) audioPlaybackProgress += seekRelativePos;
                            if (audioLoaded) ImGui::SameLine();
                            if (audioLoaded) ImGui::SliderFloat("##audioPlaybackSlider", &audioPlaybackProgress, 0, audioPlaybackLength, formatToTimestamp(audioPlaybackProgress * 60, 60).c_str(), 0);
                            if (asset.type == TextureUnionType::Video) {
                                VideoMetadata video = std::get<VideoMetadata>(asset.as);
                                assetDecoder.frame = std::ceil(glm::ceil(audioPlaybackProgress * video.framerate));
                                assetDecoder.GetGPUTexture(&asset);
                            }
                            if (audioLoaded && ImGui::IsItemEdited() || seekRequested) {
                                Servers::AudioServerRequest({
                                    {"action", "seek_sample"},
                                    {"handle", audioHandle},
                                    {"seek", (double) audioPlaybackProgress}
                                });
                            }
                            if (!audioLoaded) {
                                ImGui::Text("%s %s", ICON_FA_SPINNER, ELECTRON_GET_LOCALIZATION("IMPORTING_AUDIO"));
                            }
                            break;
                        }
                    }
                    ImGui::EndTable();
                    previousAssetID = asset.id;
                }
                ImGui::EndChild();

            UI::End();
        } else if (Shared::assetSelected == -1 || Shared::assetSelected == -128) {
            UI::Begin(CSTR(string_format("%s %s", ICON_FA_MAGNIFYING_GLASS, ELECTRON_GET_LOCALIZATION("ASSET_MANAGER_ASSET_EXAMINER"))), Signal::_CloseWindow, ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoScrollbar);
                ImVec2 windowSize = ImGui::GetContentRegionAvail();
                std::string nothingToExamine = ELECTRON_GET_LOCALIZATION("ASSET_MANAGER_NOTHING_TO_EXAMINE");
                ImVec2 textSize = ImGui::CalcTextSize(CSTR(nothingToExamine));
                ImGui::SetCursorPos({windowSize.x / 2.0f - textSize.x / 2.0f, windowSize.y / 2.0f - textSize.y / 2.0f});
                ImGui::Text("%s", CSTR(nothingToExamine));
            UI::End();
        }
    }
}