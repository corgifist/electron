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

    ELECTRON_EXPORT void UIRender(AppInstance* instance) {
        ImGui::SetCurrentContext(instance->context);
        static ResolutionVariant resolutionVariants[9];
        static bool firstSetup = true;
        static GLuint channel_manipulator = Shared::graphics->CompileComputeShader("preview_channel_manipulator.compute");


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
        UI::Begin((std::string(ICON_FA_IMAGE " ") + ELECTRON_GET_LOCALIZATION("RENDER_PREVIEW_WINDOW_TITLE") + std::string("##") + std::to_string(UICounters::RenderPreviewCounter)).c_str(), Signal::_CloseWindow, instance->isNativeWindow ? ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize : ImGuiWindowFlags_NoCollapse);
            ImVec2 windowSize = ImGui::GetWindowSize();
            if (!instance->projectOpened) {
                std::string projectRequiredString = ELECTRON_GET_LOCALIZATION("RENDER_PREVIEW_PROJECT_REQUIRED_WARNING");
                ImVec2 warningTextSize = ImGui::CalcTextSize(CSTR(projectRequiredString));

                ImGui::SetCursorPos(ImVec2{windowSize.x / 2.0f - warningTextSize.x / 2.0f,  windowSize.y / 2.0f - warningTextSize.y / 2.0f});
                ImGui::Text("%s", CSTR(projectRequiredString));
                UI::End();
                return;
            }
            static int selectedChannel = JSON_AS_TYPE(Shared::project.propertiesMap["LastColorBuffer"], int);
            Shared::project.propertiesMap["LastColorBuffer"] = selectedChannel;

            bool resizeLerpEnabled = JSON_AS_TYPE(Shared::configMap["ResizeInterpolation"], bool);

            static bool looping = JSON_AS_TYPE(Shared::project.propertiesMap["LoopPlayback"], bool);
            static int selectedResolutionVariant = JSON_AS_TYPE(Shared::project.propertiesMap["PreviewResolution"], int);
            static float previewScale = JSON_AS_TYPE(Shared::project.propertiesMap["RenderPreviewScale"], float);
            static float defaultPreviewScale = 0.7f;
            if (ImGui::Shortcut(ImGuiKey_ModCtrl | ImGuiKey_KeypadAdd)) {
                previewScale += 0.05f;
            }
            if (ImGui::Shortcut(ImGuiKey_ModCtrl | ImGuiKey_KeypadSubtract)) {
                previewScale -= 0.05f;
            }

            if (ImGui::Shortcut(ImGuiKey_ModCtrl | ImGuiKey_R)) {
                previewScale = defaultPreviewScale;
            }

            if (Shared::graphics->isPlaying) {
                if ((int) Shared::graphics->renderFrame >= Shared::graphics->renderLength) {
                    if (looping) {
                        Shared::graphics->renderFrame = 0.0f;
                        Shared::graphics->FireTimelineSeek();
                        Shared::graphics->FirePlaybackChange();
                    } else Shared::graphics->isPlaying = false;
                } else {
                    if (Shared::graphics->renderFrame < Shared::graphics->renderLength) {
                        Shared::graphics->renderFrame += (60.0f * Shared::deltaTime) * (Shared::graphics->renderFramerate / 60.0f);
                    }
                }
            }

            PipelineFrameBuffer* rbo = &Shared::graphics->renderBuffer;
            if (firstSetup) {
                RebuildPreviewResolutions(resolutionVariants, {(float) rbo->width, (float) rbo->height});
                Shared::graphics->isPlaying = JSON_AS_TYPE(Shared::project.propertiesMap["Playing"], bool);
                Shared::graphics->renderFrame = JSON_AS_TYPE(Shared::project.propertiesMap["TimelineValue"], int);
                firstSetup = false;
            }

            Shared::project.propertiesMap["Playing"] = Shared::graphics->isPlaying;
            
            GLuint previewTexture = Shared::graphics->GetPreviewGPUTexture();
            Shared::project.propertiesMap["LoopPlayback"] = looping;
            Shared::project.propertiesMap["TimelineValue"] = Shared::graphics->renderFrame;
            Shared::project.propertiesMap["PreviewResolution"] = selectedResolutionVariant;
            Shared::project.propertiesMap["RenderPreviewScale"] = previewScale;

            float windowAspectRatio = windowSize.x / windowSize.y;
            windowAspectRatio = glm::clamp(windowAspectRatio, 0.0f, 1.0f);
            ImVec2 previewTextureSize = FitRectInRect(windowSize, {(float) resolutionVariants[0].width, (float) resolutionVariants[0].height}) * previewScale;
            Shared::graphics->renderFramerate = JSON_AS_TYPE(Shared::project.propertiesMap["Framerate"], int);
            
            std::vector<float> renderTimes{};
            Shared::graphics->RequestRenderBufferCleaningWithinRegion();
            renderTimes = Shared::graphics->RequestRenderWithinRegion();

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

                /*
                Shared::graphics->UseShader(channel_manipulator);
                Shared::graphics->BindGPUTexture(previewTexture, 0, GL_READ_ONLY);
                Shared::graphics->BindGPUTexture(previewTexture, 1, GL_WRITE_ONLY);
                glm::vec4 factor{};
                if (selectedChannel == 0) factor = {1, 0, 0, 1};
                if (selectedChannel == 1) factor = {0, 1, 0, 1};
                if (selectedChannel == 2) factor = {0, 0, 1, 1};
                if (selectedChannel == 3) factor = {1, 1, 1, 1};
                Shared::graphics->ShaderSetUniform(channel_manipulator, "factor", factor);
                Shared::graphics->DispatchComputeShader(rbo->width, rbo->height, 1);
                Shared::graphics->ComputeMemoryBarier(GL_ALL_BARRIER_BITS);
                */

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
                    if (ImGui::Button(string_format("%s %s", Shared::graphics->isPlaying ? ICON_FA_PAUSE : ICON_FA_PLAY, 
                            ELECTRON_GET_LOCALIZATION(Shared::graphics->isPlaying ? "RENDER_PREVIEW_PAUSE" 
                                                                                : "RENDER_PREVIEW_PLAY")).c_str())) {
                        Shared::graphics->isPlaying = !Shared::graphics->isPlaying;
                        Shared::graphics->FirePlaybackChange();
                    }
                    ImGui::SameLine();
                    ImGui::Text("%s", ICON_FA_EXPAND);
                    ImGui::SameLine();
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
                        ImGui::SetTooltip("%s", ELECTRON_GET_LOCALIZATION("RENDER_PREVIEW_RESOLUTIONS_LABEL"));
                    }
                    ImGui::TableNextColumn();
                    static std::string outputBufferTypes[] = {
                        ELECTRON_GET_LOCALIZATION("RENDER_PREVIEW_COLOR_BUFFER"),
                        ELECTRON_GET_LOCALIZATION("RENDER_PREVIEW_UV_BUFFER"),
                        ELECTRON_GET_LOCALIZATION("RENDER_PREVIEW_DEPTH_BUFFER")
                    };
                    static PreviewOutputBufferType rawBufferTypes[] = {
                        PreviewOutputBufferType_Color,
                        PreviewOutputBufferType_UV,
                        PreviewOutputBufferType_Depth
                    };
                    static int selectedOutputBufferType = 0;
                    ImGui::Text("%s", ICON_FA_IMAGES);
                    ImGui::SameLine();
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
                        ImGui::SetTooltip("%s", ELECTRON_GET_LOCALIZATION("RENDER_PREVIEW_OUTPUT_BUFFER_TYPE"));
                    }
                    Shared::graphics->outputBufferType = rawBufferTypes[selectedOutputBufferType];
                    ImGui::TableNextColumn();
                    static std::string channelTypes[] = {
                        ELECTRON_GET_LOCALIZATION("GENERIC_R"),
                        ELECTRON_GET_LOCALIZATION("GENERIC_G"),
                        ELECTRON_GET_LOCALIZATION("GENERIC_B"),
                        ELECTRON_GET_LOCALIZATION("GENERIC_RGB")
                    };
                    ImGui::Text("%s", ICON_FA_DROPLET);
                    ImGui::SameLine();
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
                        ImGui::SetTooltip("%s", ELECTRON_GET_LOCALIZATION("RENDER_PREVIEW_OUTPUT_CHANNELS"));
                    }

                    ImGui::TableNextColumn();
                    ImGui::Text("%s", ICON_FA_STOPWATCH);
                    ImGui::SameLine();
                    static std::vector<float> plottingRenderAverages(90);
                    static int plottingCounter = 0;
                    float renderAverage = 0;
                    for (int i = 0; i < Shared::graphics->layers.size(); i++) {
                        RenderLayer& layer = Shared::graphics->layers[i];
                        float renderTime = renderTimes[i];
                        renderAverage += renderTime;
                    }
                    renderAverage = renderAverage / Shared::graphics->layers.size();
                    if (plottingCounter == plottingRenderAverages.size()) 
                        plottingCounter = 0;
                    plottingRenderAverages[plottingCounter++] = renderAverage;

                    ImGui::PlotLines("##renderStatictics", plottingRenderAverages.data(), plottingRenderAverages.size(), plottingCounter, NULL, 0.001, 0.033);

                    ImGui::EndTable();
                }
                propertiesHeight = ImGui::GetCursorPosY() - firstCursor;
            ImGui::EndChild();
        UI::End();
        std::vector<int> projectResolution = JSON_AS_TYPE(Shared::project.propertiesMap["ProjectResolution"], std::vector<int>);
        ResolutionVariant currentVariant = resolutionVariants[selectedResolutionVariant];
        if ((currentVariant.width != rbo->width || currentVariant.height != rbo->height)) {
            Shared::graphics->ResizeRenderBuffer(currentVariant.width, currentVariant.height);
            Shared::graphics->RequestRenderBufferCleaningWithinRegion(); // fixes flickering when resizing rbo
        }
        if ((resolutionVariants[0].width != projectResolution[0] || resolutionVariants[0].height != projectResolution[1])) {
            Shared::graphics->ResizeRenderBuffer(projectResolution[0], projectResolution[1]);

            RebuildPreviewResolutions(resolutionVariants, {(float) projectResolution[0], (float) projectResolution[1]});

            ResolutionVariant selectedVariant = resolutionVariants[selectedResolutionVariant];
            Shared::graphics->ResizeRenderBuffer(selectedVariant.width, selectedVariant.height);
        }
        internalFrameIndex++;
    }
}