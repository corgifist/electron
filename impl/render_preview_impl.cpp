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

        this->raw = ImVec2{(float) width, (float) height};
    }

    ResolutionVariant() {};
};

extern "C" {
    void RebuildPreviewResolutions(ResolutionVariant* resolutionVariants, ImVec2 newResolution) {
        ImVec2 fractSize = newResolution / 10.0f;
        ImVec2 sizeAcc = newResolution;

        for (int i = 0; i < 9; i++) {
            ResolutionVariant variant{(int) sizeAcc.x, (int) sizeAcc.y};
            resolutionVariants[i] = variant;
            sizeAcc -= fractSize;
        }
    }

    static void DrawRect(RectBounds bounds, ImVec4 color) {
        ImGui::GetWindowDrawList()->AddRectFilled(bounds.UL, bounds.BR, ImGui::ColorConvertFloat4ToU32(color));
    }

    ELECTRON_EXPORT void RenderPreviewRender(AppInstance* instance, RenderPreview* owner) {
        ImGui::SetCurrentContext(instance->context);
        static ResolutionVariant resolutionVariants[9];
        static bool firstSetup = true;
        static GLuint channel_manipulator = instance->graphics.CompileComputeShader("preview_channel_manipulator.compute");


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

                ImGui::SetCursorPos(ImVec2{windowSize.x / 2.0f - warningTextSize.x / 2.0f,  windowSize.y / 2.0f - warningTextSize.y / 2.0f});
                ImGui::Text(CSTR(projectRequiredString));
                UI::End();
                return;
            }
            static int selectedChannel = JSON_AS_TYPE(instance->project.propertiesMap["LastColorBuffer"], int);
            instance->project.propertiesMap["LastColorBuffer"] = selectedChannel;

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
            if (playing) {
                if ((int) instance->graphics.renderFrame >= instance->graphics.renderLength) {
                    if (looping) {
                        instance->graphics.renderFrame = 0.0f;
                    } else playing = false;
                } else {
                    if (instance->graphics.renderFrame < instance->graphics.renderLength) {
                        float intFrame = 1.0f / (60.0 / instance->graphics.renderFramerate);
                        instance->graphics.renderFrame += intFrame;
                    }
                }
            }

            RenderBuffer* rbo = &instance->graphics.renderBuffer;
            if (firstSetup) {
                RebuildPreviewResolutions(resolutionVariants, {(float) rbo->width, (float) rbo->height});
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
            ImVec2 previewTextureSize = FitRectInRect(windowSize, {(float) resolutionVariants[0].width, (float) resolutionVariants[0].height}) * previewScale;
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
            static ImVec2 previousWindowPos = ImGui::GetWindowPos();
            static ImVec2 previousWindowSize = ImGui::GetWindowSize();
            ImVec2 wSize = ImGui::GetWindowSize();
            ImVec2 wPos = ImGui::GetWindowPos();
            ImGui::BeginChild("previewZone", ImVec2(ImGui::GetContentRegionAvail().x, ImGui::GetContentRegionAvail().y - propertiesHeight), false, 0);
                windowSize = ImGui::GetContentRegionAvail();
                imagePreviewDrag.Activate();
            
                float imageDragDistance;

                if (imagePreviewDrag.GetDragDistance(imageDragDistance) && (previousWindowPos == wPos) && (previousWindowSize == wSize)) {
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

                instance->graphics.UseShader(channel_manipulator);
                instance->graphics.BindGPUTexture(previewTexture, 0, GL_READ_ONLY);
                instance->graphics.BindGPUTexture(previewTexture, 1, GL_WRITE_ONLY);
                glm::vec4 factor{};
                if (selectedChannel == 0) factor = {1, 0, 0, 1};
                if (selectedChannel == 1) factor = {0, 1, 0, 1};
                if (selectedChannel == 2) factor = {0, 0, 1, 1};
                if (selectedChannel == 3) factor = {1, 1, 1, 1};
                instance->graphics.ShaderSetUniform(channel_manipulator, "factor", factor);
                instance->graphics.DispatchComputeShader(rbo->width, rbo->height, 1);
                instance->graphics.ComputeMemoryBarier(GL_ALL_BARRIER_BITS);


                ImGui::SetCursorPos(ImVec2{windowSize.x / 2.0f - previewTextureSize.x / 2.0f, windowSize.y / 2.0f - previewTextureSize.y / 2.0f} + imageOffset);
                ImGui::Image((ImTextureID)(uint64_t) previewTexture, previewTextureSize);
            ImGui::EndChild();
            previousWindowPos = ImGui::GetWindowPos();
            previousWindowSize = ImGui::GetWindowSize();

            ImGui::BeginChild("propertiesZone", ImVec2(ImGui::GetContentRegionAvail().x, propertiesHeight), false, 0);
                windowSize = ImGui::GetContentRegionAvail();
                float firstCursor = ImGui::GetCursorPosY();

                if (ImGui::BeginTable("propertiesTable", 4)) {
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
                    static std::string channelTypes[] = {
                        ELECTRON_GET_LOCALIZATION(instance, "GENERIC_R"),
                        ELECTRON_GET_LOCALIZATION(instance, "GENERIC_G"),
                        ELECTRON_GET_LOCALIZATION(instance, "GENERIC_B"),
                        ELECTRON_GET_LOCALIZATION(instance, "GENERIC_RGB")
                    };

                    if (ImGui::BeginCombo("##channelType", CSTR(channelTypes[selectedChannel]))) {
                        for (int i = 0; i < 4; i++) {
                            bool channelSelected = i == selectedChannel;
                            if (ImGui::Selectable(CSTR(channelTypes[i]), channelSelected))
                                selectedChannel = i;
                            if (channelSelected) ImGui::SetItemDefaultFocus();
                        }
                        ImGui::EndCombo();
                    }
                    if (ImGui::IsItemHovered()) {
                        ImGui::SetTooltip(ELECTRON_GET_LOCALIZATION(instance, "RENDER_PREVIEW_OUTPUT_CHANNELS"));
                    }

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

            RebuildPreviewResolutions(resolutionVariants, {(float) projectResolution[0], (float) projectResolution[1]});

            ResolutionVariant selectedVariant = resolutionVariants[selectedResolutionVariant];
            instance->graphics.ResizeRenderBuffer(selectedVariant.width, selectedVariant.height);
        }
        internalFrameIndex++;
    }
}