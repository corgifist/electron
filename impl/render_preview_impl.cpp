#include "editor_core.h"
#include "app.h"
#include "utils/drag_utils.h"
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

    GPUExtendedHandle renderBufferColorHandle = 0, renderBufferUVHandle = 0;
    ResolutionVariant resolutionVariants[9];

    static void RebuildRenderBufferHandles() {
        if (renderBufferColorHandle) DriverCore::DestroyImageHandleUI(renderBufferColorHandle);
        if (renderBufferUVHandle) DriverCore::DestroyImageHandleUI(renderBufferUVHandle);

        renderBufferColorHandle = DriverCore::GetImageHandleUI(GraphicsCore::renderBuffer.rbo.colorBuffer);
        renderBufferUVHandle = DriverCore::GetImageHandleUI(GraphicsCore::renderBuffer.rbo.uvBuffer);
    }

    ELECTRON_EXPORT void UIRender() {
        ImGui::SetCurrentContext(AppCore::context);
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

        PipelineFrameBuffer* rbo = &GraphicsCore::renderBuffer;
        static int selectedResolutionVariant = 0;

        if (firstSetup && AppCore::projectOpened) {
            selectedResolutionVariant = JSON_AS_TYPE(Shared::project.propertiesMap["PreviewResolution"], int);
            RebuildPreviewResolutions(resolutionVariants, {(float) rbo->width, (float) rbo->height});
            RebuildRenderBufferHandles();
            GraphicsCore::isPlaying = JSON_AS_TYPE(Shared::project.propertiesMap["Playing"], bool);
            GraphicsCore::renderFrame = JSON_AS_TYPE(Shared::project.propertiesMap["TimelineValue"], int);
            std::vector<float> imageOffsetVector = Shared::project.propertiesMap["RenderPreviewOffset"];
            imageOffset = {imageOffsetVector[0], imageOffsetVector[1]};
            firstSetup = false;
        }

        UI::Begin((std::string(ICON_FA_IMAGE " ") + ELECTRON_GET_LOCALIZATION("RENDER_PREVIEW_WINDOW_TITLE") + std::string("##") + std::to_string(UICounters::RenderPreviewCounter)).c_str(), Signal::_CloseWindow, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
            ImVec2 windowSize = ImGui::GetWindowSize();
            if (!AppCore::projectOpened) {
                std::string projectRequiredString = ELECTRON_GET_LOCALIZATION("RENDER_PREVIEW_PROJECT_REQUIRED_WARNING");
                ImVec2 warningTextSize = ImGui::CalcTextSize(CSTR(projectRequiredString));

                ImGui::SetCursorPos(ImVec2{windowSize.x / 2.0f - warningTextSize.x / 2.0f,  windowSize.y / 2.0f - warningTextSize.y / 2.0f});
                ImGui::Text("%s", CSTR(projectRequiredString));
                std::string lastProject = JSON_AS_TYPE(Shared::configMap["LastProject"], std::string);
                if (lastProject != "null" && std::filesystem::exists(lastProject + "/project.json")) {
                    static json_t loadedProject;
                    static std::string loadedProjectPath = "null";
                    if (loadedProjectPath != lastProject) {
                        loadedProjectPath = lastProject;
                        loadedProject = json_t::parse(read_file(lastProject + "/project.json"));
                    }
                    ProjectMap project;
                    project.path = loadedProjectPath;
                    project.propertiesMap = loadedProject;
                    if (AppCore::ButtonCenteredOnLine(string_format("%s %s: '%s'", ICON_FA_FOLDER_OPEN, ELECTRON_GET_LOCALIZATION("PROJECT_CONFIGURATION_OPEN_RECENT_PROJECT"), JSON_AS_TYPE(loadedProject["ProjectName"], std::string).c_str()).c_str())) {
                        Shortcuts::Ctrl_P_O(project);
                    }
                }
                UI::End();
                return;
            }

            std::vector<int> projectResolution;
            ResolutionVariant currentVariant;
            if (AppCore::projectOpened) {
                projectResolution = JSON_AS_TYPE(Shared::project.propertiesMap["ProjectResolution"], std::vector<int>);
                currentVariant = resolutionVariants[selectedResolutionVariant];
            }
            if (AppCore::projectOpened && (currentVariant.width != rbo->width || currentVariant.height != rbo->height)) {
                GraphicsCore::ResizeRenderBuffer(currentVariant.width, currentVariant.height);
                RebuildRenderBufferHandles();
            }
            if (AppCore::projectOpened && (resolutionVariants[0].width != projectResolution[0] || resolutionVariants[0].height != projectResolution[1])) {
                GraphicsCore::ResizeRenderBuffer(projectResolution[0], projectResolution[1]);
                RebuildRenderBufferHandles();

                RebuildPreviewResolutions(resolutionVariants, {(float) projectResolution[0], (float) projectResolution[1]});
                
                ResolutionVariant selectedVariant = resolutionVariants[selectedResolutionVariant];
                GraphicsCore::ResizeRenderBuffer(selectedVariant.width, selectedVariant.height);

                RebuildRenderBufferHandles();
            }
            static int selectedChannel = JSON_AS_TYPE(Shared::project.propertiesMap["LastColorBuffer"], int);
            Shared::project.propertiesMap["LastColorBuffer"] = selectedChannel;

            bool resizeLerpEnabled = JSON_AS_TYPE(Shared::configMap["ResizeInterpolation"], bool);

            static bool looping = JSON_AS_TYPE(Shared::project.propertiesMap["LoopPlayback"], bool);
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

            if (GraphicsCore::isPlaying) {
                if ((int) GraphicsCore::renderFrame >= GraphicsCore::renderLength) {
                    if (looping) {
                        GraphicsCore::renderFrame = 0.0f;
                        GraphicsCore::FireTimelineSeek();
                        GraphicsCore::FirePlaybackChange();
                    } else GraphicsCore::isPlaying = false;
                } else {
                    if (GraphicsCore::renderFrame < GraphicsCore::renderLength) {
                        GraphicsCore::renderFrame += (GraphicsCore::renderFramerate * Shared::deltaTime);
                    }
                }
            }

            Shared::project.propertiesMap["Playing"] = GraphicsCore::isPlaying;
            
            GPUExtendedHandle previewTexture = GraphicsCore::outputBufferType == PreviewOutputBufferType_Color ? 
                                        renderBufferColorHandle : renderBufferUVHandle;
            Shared::project.propertiesMap["LoopPlayback"] = looping;
            Shared::project.propertiesMap["TimelineValue"] = GraphicsCore::renderFrame;
            Shared::project.propertiesMap["PreviewResolution"] = selectedResolutionVariant;
            Shared::project.propertiesMap["RenderPreviewScale"] = previewScale;

            float windowAspectRatio = windowSize.x / windowSize.y;
            windowAspectRatio = glm::clamp(windowAspectRatio, 0.0f, 1.0f);
            ImVec2 previewTextureSize = FitRectInRect(windowSize, {(float) resolutionVariants[0].width, (float) resolutionVariants[0].height}) * previewScale;
            GraphicsCore::renderFramerate = JSON_AS_TYPE(Shared::project.propertiesMap["Framerate"], int);
            
            std::vector<float> renderTimes{};
            GraphicsCore::RequestRenderBufferCleaningWithinRegion();
            renderTimes = GraphicsCore::RequestRenderWithinRegion();

            static float propertiesHeight = 50;
            static ImVec2 previousWindowPos = ImGui::GetWindowPos();
            static ImVec2 previousWindowSize = ImGui::GetWindowSize();
            ImVec2 wSize = ImGui::GetWindowSize();
            ImVec2 wPos = ImGui::GetWindowPos();
            ImGui::BeginChild("previewZone", ImVec2(ImGui::GetContentRegionAvail().x, ImGui::GetContentRegionAvail().y - propertiesHeight), false, 0);
                windowSize = ImGui::GetContentRegionAvail();
                imagePreviewDrag.Activate();
            
                ImGui::SetCursorPos({0, 0});
                ImGui::Text("%s: %i; %s: %i", ELECTRON_GET_LOCALIZATION("RENDER_CALLS"), Shared::renderCalls, ELECTRON_GET_LOCALIZATION("COMPOSITOR_CALLS"), Shared::compositorCalls);

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
                
                ImVec4 tint = {1, 1, 1, 1};
                if (selectedChannel == 3) tint = {1, 1, 1, 1};
                if (selectedChannel == 0) tint = {1, 0, 0, 1};
                if (selectedChannel == 1) tint = {0, 1, 0, 1};
                if (selectedChannel == 2) tint = {0, 0, 1, 1};

                ImGui::SetCursorPos(ImVec2{windowSize.x / 2.0f - previewTextureSize.x / 2.0f, windowSize.y / 2.0f - previewTextureSize.y / 2.0f} + imageOffset);
                ImGui::Image((ImTextureID) previewTexture, previewTextureSize, ImVec2(0, 0), ImVec2(1, 1), tint);
            ImGui::EndChild();
            previousWindowPos = ImGui::GetWindowPos();
            previousWindowSize = ImGui::GetWindowSize();

            ImGui::BeginChild("propertiesZone", ImVec2(ImGui::GetContentRegionAvail().x, propertiesHeight), false, 0);
                windowSize = ImGui::GetContentRegionAvail();
                float firstCursor = ImGui::GetCursorPosY();

                if (ImGui::BeginTable("propertiesTable", 4)) {
                    ImGui::TableNextColumn();
                    if (ImGui::Button(string_format("%s %s", GraphicsCore::isPlaying ? ICON_FA_PAUSE : ICON_FA_PLAY, 
                            ELECTRON_GET_LOCALIZATION(GraphicsCore::isPlaying ? "RENDER_PREVIEW_PAUSE" 
                                                                                : "RENDER_PREVIEW_PLAY")).c_str())) {
                        GraphicsCore::isPlaying = !GraphicsCore::isPlaying;
                        GraphicsCore::FirePlaybackChange();
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
                        ELECTRON_GET_LOCALIZATION("RENDER_PREVIEW_UV_BUFFER")
                    };
                    static PreviewOutputBufferType rawBufferTypes[] = {
                        PreviewOutputBufferType_Color,
                        PreviewOutputBufferType_UV
                    };
                    static int selectedOutputBufferType = 0;
                    ImGui::Text("%s", ICON_FA_IMAGES);
                    ImGui::SameLine();
                    if (ImGui::BeginCombo("##previewTextureType", CSTR(outputBufferTypes[selectedOutputBufferType]))) {
                        for (int i = 0; i < 2; i++) {
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
                    GraphicsCore::outputBufferType = rawBufferTypes[selectedOutputBufferType];
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
                    for (int i = 0; i < GraphicsCore::layers.size(); i++) {
                        RenderLayer& layer = GraphicsCore::layers[i];
                        float renderTime = renderTimes[i];
                        renderAverage += renderTime;
                    }
                    renderAverage = renderAverage / GraphicsCore::layers.size();
                    if (plottingCounter == plottingRenderAverages.size()) 
                        plottingCounter = 0;
                    plottingRenderAverages[plottingCounter++] = renderAverage;

                    ImGui::PlotLines("##renderStatictics", plottingRenderAverages.data(), plottingRenderAverages.size(), plottingCounter, NULL, 0.001, 0.033);

                    ImGui::EndTable();
                }
                propertiesHeight = ImGui::GetCursorPosY() - firstCursor;
            ImGui::EndChild();
        UI::End();
        internalFrameIndex++;
    }
}