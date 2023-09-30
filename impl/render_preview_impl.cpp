#include "editor_core.h"
#include "app.h"
#include "drag_utils.h"
#define CSTR(x) ((x).c_str())

using namespace Electron;

struct ResolutionVariant {
    int width, height;
    std::string repr;
    ImVec2 raw;

    ResolutionVariant(int width, int height) {
        this->width = width;
        this->height = height;

        this->repr = std::to_string(width) + "x" + std::to_string(height);

        this->raw = ImVec2{width, height};
    }

    ResolutionVariant() {};
};

extern "C" {
    void RebuildPreviewResolutions(ResolutionVariant* resolutionVariants, ImVec2 newResolution) {
        ImVec2 fractSize = newResolution / 10.0f;
        ImVec2 sizeAcc = newResolution;

        for (int i = 0; i < 9; i++) {
            ResolutionVariant variant{sizeAcc.x, sizeAcc.y};
            resolutionVariants[i] = variant;
            sizeAcc -= fractSize;
        }
    }

    static ImVec2 FitRectInRect(ImVec2 screen, ImVec2 rectangle) {
        ImVec2 dst = screen;
        ImVec2 src = rectangle;
        float scale = glm::min(dst.x / src.x, dst.y / src.y);
        return ImVec2{src.x * scale, src.y * scale};
    }

    static void DrawRect(RectBounds bounds, ImVec4 color) {
        ImGui::GetWindowDrawList()->AddRectFilled(bounds.UL, bounds.BR, ImGui::ColorConvertFloat4ToU32(color));
    }

    ELECTRON_EXPORT void RenderPreviewRender(AppInstance* instance, RenderPreview* owner) {
        ImGui::SetCurrentContext(instance->context);
        static ResolutionVariant resolutionVariants[9];
        static bool firstSetup = true;


        static int internalFrameIndex = 0;

        static ImVec2 targetResizeResolution{};
        static ImVec2 beginResizeResolution{};
        static float reiszeLerpPercentage = 0;
        static bool beginInterpolation = false;
        static bool rebuildResolutions = false;
        ImGuiStyle& style = ImGui::GetStyle();



        ImGui::SetNextWindowSize({640, 480}, ImGuiCond_Once);
        static DragStructure imagePreviewDrag{};
        static ImVec2 imageOffset{0, 0};
        UI::Begin((std::string(ICON_FA_IMAGE " ") + ELECTRON_GET_LOCALIZATION(instance, "RENDER_PREVIEW_WINDOW_TITLE") + std::string("##") + std::to_string(UICounters::RenderPreviewCounter)).c_str(), ElectronSignal_CloseWindow, instance->isNativeWindow ? ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize : ImGuiWindowFlags_NoCollapse);
            ImVec2 windowSize = ImGui::GetWindowSize();
            if (!instance->projectOpened) {
                std::string projectRequiredString = ELECTRON_GET_LOCALIZATION(instance, "RENDER_PREVIEW_PROJECT_REQUIRED_WARNING");
                ImVec2 warningTextSize = ImGui::CalcTextSize(CSTR(projectRequiredString));

                ImGui::SetCursorPos(ImVec2{windowSize.x / 2.0 - warningTextSize.x / 2.0f,  windowSize.y / 2.0 - warningTextSize.y / 2.0});
                ImGui::Text(CSTR(projectRequiredString));
                UI::End();
                return;
            }

            bool resizeLerpEnabled = JSON_AS_TYPE(instance->configMap["ResizeInterpolation"], bool);

            static bool playing = false;
            static bool looping = JSON_AS_TYPE(instance->project.propertiesMap["LoopPlayback"], bool);
            static int selectedResolutionVariant = JSON_AS_TYPE(instance->project.propertiesMap["PreviewResolution"], int);
            static float previewScale = JSON_AS_TYPE(instance->project.propertiesMap["RenderPreviewScale"], float);
            if (ImGui::Shortcut(ImGuiKey_ModCtrl | ImGuiKey_KeypadAdd)) {
                previewScale += 0.05f;
            }
            if (ImGui::Shortcut(ImGuiKey_ModCtrl | ImGuiKey_KeypadSubtract)) {
                previewScale -= 0.05f;
            }

            if (instance->graphics.firePlay) {
                playing = !playing;
                instance->graphics.firePlay = false;
            }
            instance->graphics.isPlaying = playing;

            RenderBuffer* rbo = &instance->graphics.renderBuffer;
            if (firstSetup) {
                RebuildPreviewResolutions(resolutionVariants, {rbo->width, rbo->height});
                instance->graphics.renderFrame = JSON_AS_TYPE(instance->project.propertiesMap["TimelineValue"], int);
                firstSetup = false;
            }
            
            GLuint previewTexture = instance->graphics.GetPreviewBufferByOutputType();
            instance->project.propertiesMap["LoopPlayback"] = looping;
            instance->project.propertiesMap["TimelineValue"] = instance->graphics.renderFrame;
            instance->project.propertiesMap["PreviewResolution"] = selectedResolutionVariant;
            instance->project.propertiesMap["RenderPreviewScale"] = previewScale;

            float windowAspectRatio = windowSize.x / windowSize.y;
            windowAspectRatio = glm::clamp(windowAspectRatio, 0.0f, 1.0f);
            ImVec2 previewTextureSize = FitRectInRect(windowSize, {resolutionVariants[0].width, resolutionVariants[0].height}) * previewScale;
            instance->graphics.renderFramerate = JSON_AS_TYPE(instance->project.propertiesMap["Framerate"], int);
            
            RenderRequestMetadata metadata;
            metadata.backgroundColor = JSON_AS_TYPE(instance->project.propertiesMap["BackgroundColor"], std::vector<float>);
            metadata.beginX = 0;
            metadata.beginY = 0;
            metadata.endX = rbo->width;
            metadata.endY = rbo->height;
            std::vector<float> renderTimes{};
            instance->graphics.RequestRenderBufferCleaningWithinRegion(metadata);
            renderTimes = instance->graphics.RequestRenderWithinRegion(metadata);

            static float propertiesHeight = 50;
            ImGui::BeginChild("previewZone", ImVec2(ImGui::GetContentRegionAvail().x, ImGui::GetContentRegionAvail().y - propertiesHeight), false, 0);
                windowSize = ImGui::GetContentRegionAvail();
                imagePreviewDrag.Activate();
            
                float imageDragDistance;

                if (imagePreviewDrag.GetDragDistance(imageDragDistance)) {
                    ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
                    imageOffset = imageOffset + ImGui::GetIO().MouseDelta;
                    DrawRect(RectBounds(ImVec2(windowSize.x / 2.0f + imageOffset.x - 2, 0), ImVec2(4.0f, ImGui::GetWindowSize().y)), style.Colors[ImGuiCol_Border]);
                    DrawRect(RectBounds(ImVec2(0, windowSize.y / 2.0f + imageOffset.y - 2), ImVec2(ImGui::GetWindowSize().x, 4.0f)), style.Colors[ImGuiCol_Border]);
                } else imagePreviewDrag.Deactivate();
                if (glm::abs(imageOffset.x) < 5) {
                    imageOffset.x = 0;
                }
                if (glm::abs(imageOffset.y) < 5) {
                    imageOffset.y = 0;
                }
                ImGui::SetCursorPos(ImVec2{windowSize.x / 2.0f - previewTextureSize.x / 2.0f, windowSize.y / 2.0f - previewTextureSize.y / 2.0f} + imageOffset);
                ImGui::Image((ImTextureID) previewTexture, previewTextureSize);
            ImGui::EndChild();
            
            ImGui::BeginChild("propertiesZone", ImVec2(ImGui::GetContentRegionAvail().x, propertiesHeight), false, 0);
                windowSize = ImGui::GetContentRegionAvail();
                float firstCursor = ImGui::GetCursorPosY();

                if (ImGui::BeginTable("propertiesTable", 3)) {
                    ImGui::TableNextColumn();
                    if (ImGui::BeginCombo("##previewResolutions", resolutionVariants[selectedResolutionVariant].repr.c_str())) {
                        for (int n = 0; n < 9; n++) {
                            bool resolutionSelected = (n == selectedResolutionVariant);
                            if (ImGui::Selectable(resolutionVariants[n].repr.c_str(), resolutionSelected)) {
                                selectedResolutionVariant = n;
                            }
                            if (resolutionSelected) ImGui::SetItemDefaultFocus();
                        }
                        ImGui::EndCombo();
                    }
                    if (ImGui::IsItemHovered()) {
                        ImGui::SetTooltip(ELECTRON_GET_LOCALIZATION(instance, "RENDER_PREVIEW_RESOLUTIONS_LABEL"));
                    }
                    ImGui::TableNextColumn();
                    static std::string outputBufferTypes[] = {
                        ELECTRON_GET_LOCALIZATION(instance, "RENDER_PREVIEW_COLOR_BUFFER"),
                        ELECTRON_GET_LOCALIZATION(instance, "RENDER_PREVIEW_UV_BUFFER"),
                        ELECTRON_GET_LOCALIZATION(instance, "RENDER_PREVIEW_DEPTH_BUFFER")
                    };
                    static PreviewOutputBufferType rawBufferTypes[] = {
                        PreviewOutputBufferType_Color,
                        PreviewOutputBufferType_UV,
                        PreviewOutputBufferType_Depth
                    };
                    static int selectedOutputBufferType = 0;
                    if (ImGui::BeginCombo("##previewTextureType", CSTR(outputBufferTypes[selectedOutputBufferType]))) {
                        for (int i = 0; i < 3; i++) {
                            bool bufferTypeSelected = i == selectedOutputBufferType;
                            if (ImGui::Selectable(CSTR(outputBufferTypes[i]), bufferTypeSelected))
                                selectedOutputBufferType = i;
                            if (bufferTypeSelected) ImGui::SetItemDefaultFocus();
                        }
                        ImGui::EndCombo();
                    }
                    if (ImGui::IsItemHovered()) {
                        ImGui::SetTooltip(ELECTRON_GET_LOCALIZATION(instance, "RENDER_PREVIEW_OUTPUT_BUFFER_TYPE"));
                    }
                    instance->graphics.outputBufferType = rawBufferTypes[selectedOutputBufferType];
                    ImGui::TableNextColumn();
                    static std::vector<float> plottingRenderAverages(90);
                    static int plottingCounter = 0;
                    float renderAverage = 0;
                    for (int i = 0; i < instance->graphics.layers.size(); i++) {
                        RenderLayer& layer = instance->graphics.layers[i];
                        float renderTime = renderTimes[i];
                        renderAverage += renderTime;
                    }
                    renderAverage = renderAverage / instance->graphics.layers.size();
                    if (plottingCounter == plottingRenderAverages.size()) 
                        plottingCounter = 0;
                    plottingRenderAverages[plottingCounter++] = renderAverage;

                    ImGui::PlotLines("##renderStatictics", plottingRenderAverages.data(), plottingRenderAverages.size(), plottingCounter, NULL, -1, 2);

                    ImGui::EndTable();
                }
                propertiesHeight = ImGui::GetCursorPosY() - firstCursor;
            ImGui::EndChild();
        UI::End();
        std::vector<int> projectResolution = JSON_AS_TYPE(instance->project.propertiesMap["ProjectResolution"], std::vector<int>);
        if ((resolutionVariants[0].width != projectResolution[0] || resolutionVariants[0].height != projectResolution[1]) && !beginInterpolation) {
            instance->graphics.ResizeRenderBuffer(projectResolution[0], projectResolution[1]);

            RebuildPreviewResolutions(resolutionVariants, {projectResolution[0], projectResolution[1]});

            ResolutionVariant selectedVariant = resolutionVariants[selectedResolutionVariant];
            instance->graphics.ResizeRenderBuffer(selectedVariant.width, selectedVariant.height);
        }
        internalFrameIndex++;
    }
}