#include "editor_core.h"
#include "ui_api.h"

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

    ELECTRON_EXPORT void RenderPreviewRender(AppInstance* instance, RenderPreview* owner) {
        static float previewScale = 1.0f;

        static ResolutionVariant resolutionVariants[9];
        static bool firstSetup = true;
        static int selectedResolutionVariant = 0;

        static int internalFrameIndex = 0;

        static ImVec2 targetResizeResolution{};
        static ImVec2 beginResizeResolution{};
        static float reiszeLerpPercentage = 0;
        static bool beginInterpolation = false;
        static bool rebuildResolutions = false;

        bool resizeLerpEnabled = JSON_AS_TYPE(instance->configMap["ResizeInterpolation"], bool);

        static bool playing = false;
        static bool looping = false;

        UISetNextWindowSize({640, 480}, ImGuiCond_Once);
        UIBegin(CSTR(ElectronImplTag(ELECTRON_GET_LOCALIZATION(instance, "RENDER_PREVIEW_WINDOW_TITLE"), owner)), ElectronSignal_CloseWindow, instance->isNativeWindow ? ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize : ImGuiWindowFlags_NoCollapse);
            ImVec2 windowSize = UIGetWindowSize();
            ImVec2 availZone = UIGetAvailZone();
            if (!instance->projectOpened) {
                std::string projectRequiredString = ELECTRON_GET_LOCALIZATION(instance, "RENDER_PREVIEW_PROJECT_REQUIRED_WARNING");
                ImVec2 warningTextSize = UICalcTextSize(CSTR(projectRequiredString));

                UISetCursorPos(ImVec2{windowSize.x / 2.0 - warningTextSize.x / 2.0f,  windowSize.y / 2.0 - warningTextSize.y / 2.0});
                UIText(CSTR(projectRequiredString));
                UIEnd();
                return;
            }
            PixelBuffer& renderBuffer = GraphicsImplGetPreviewBufferByOutputType(instance);

            if (firstSetup) {
                RebuildPreviewResolutions(resolutionVariants, {renderBuffer.width, renderBuffer.height});
                firstSetup = false;
            }

            ImVec2 scaledPreviewSize = {resolutionVariants[0].width, resolutionVariants[0].height};

            float scaleFactor = ((availZone.x / scaledPreviewSize.x * 0.1f) + (availZone.y / scaledPreviewSize.y * 0.1f)) / 2.0f;
            scaledPreviewSize.x += scaledPreviewSize.x * scaleFactor;
            scaledPreviewSize.y += scaledPreviewSize.y * scaleFactor;
            scaledPreviewSize.x *= previewScale;
            scaledPreviewSize.y *= previewScale;


            GraphicsImplCleanPreviewGPUTexture(instance);

            instance->graphics.renderLength = 60;
            instance->graphics.renderFramerate = 30;
            RenderRequestMetadata metadata;
            metadata.backgroundColor = JSON_AS_TYPE(instance->project.propertiesMap["BackgroundColor"], std::vector<float>);
            metadata.beginX = 0;
            metadata.beginY = 0;
            metadata.endX = renderBuffer.width;
            metadata.endY = renderBuffer.height;
            GraphicsImplRequestRenderWithinRegion(instance, metadata);
            GraphicsImplBuildPreviewGPUTexture(instance);
            GLuint gpuTex = GraphicsImplGetPreviewGPUTexture(instance);
            
            UISetCursorX(windowSize.x / 2.0f - scaledPreviewSize.x / 2.0f);
            UIImage(gpuTex, scaledPreviewSize);
            
            float translatedTimelineValue = (float) instance->graphics.renderFrame / (float) instance->graphics.renderFramerate;
            float translatedRenderLength = (float) instance->graphics.renderLength / (float) instance->graphics.renderFramerate;
            
            if (playing) {
                if ((int) instance->graphics.renderFrame >= instance->graphics.renderLength) {
                    if (looping) {
                        translatedTimelineValue = 0.0f;
                    } else playing = false;
                } else {
                    if (instance->graphics.renderFrame < instance->graphics.renderLength)
                        translatedTimelineValue += (1.0f / 60.0f) * (60.0f / instance->graphics.renderFramerate);
                }
            }

            if (UIButton(ELECTRON_GET_LOCALIZATION(instance, playing ? "RENDER_PREVIEW_PAUSE" : "RENDER_PREVIEW_PLAY"))) {
                playing = !playing;
            }
            UISameLine();
            UICheckbox(ELECTRON_GET_LOCALIZATION(instance, "RENDER_PREVIEW_LOOP_PLAYBACK"), &looping);
            UIPushItemWidth(windowSize.x * 0.96f);
                UISliderFloat("##", &translatedTimelineValue, 0, translatedRenderLength, CSTR(std::string("%0.") + std::to_string(JSON_AS_TYPE(instance->configMap["RenderPreviewTimelinePrescision"], int)) + "f"), 0);
            UIPopItemWidth();
            instance->graphics.renderFrame = (float) instance->graphics.renderFramerate * translatedTimelineValue;
            UISpacing();
            UISeparator();
            UISpacing();
            if (UICollapsingHeader(ELECTRON_GET_LOCALIZATION(instance, "RENDER_PREVIEW_SETTINGS_HEADER"))) {
                UISliderFloat(ELECTRON_GET_LOCALIZATION(instance, "RENDER_PREVIEW_PREVIEW_SCALE"), &previewScale, 0.1f, 2.0f, "%0.1f", 0);
                if (UIBeginCombo(ELECTRON_GET_LOCALIZATION(instance, "RENDER_PREVIEW_RESOLUTIONS_LABEL"), resolutionVariants[selectedResolutionVariant].repr.c_str())) {
                    for (int n = 0; n < 9; n++) {
                        bool resolutionSelected = (n == selectedResolutionVariant);
                        if (UISelectable(resolutionVariants[n].repr.c_str(), resolutionSelected)) {
                            if (resizeLerpEnabled) {
                                ResolutionVariant currentVariant = resolutionVariants[selectedResolutionVariant];
                                beginResizeResolution = ImVec2{currentVariant.width, currentVariant.height};
                                selectedResolutionVariant = n;
                                currentVariant = resolutionVariants[selectedResolutionVariant];
                                targetResizeResolution = ImVec2{currentVariant.width, currentVariant.height};
                                beginInterpolation = true;
                            } 
                            selectedResolutionVariant = n;
                        }
                        if (resolutionSelected) UISetItemFocusDefault();
                    }
                    UIEndCombo();
                }

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
                if (UIBeginCombo(ELECTRON_GET_LOCALIZATION(instance, "RENDER_PREVIEW_OUTPUT_BUFFER_TYPE"), CSTR(outputBufferTypes[selectedOutputBufferType]))) {
                    for (int i = 0; i < 3; i++) {
                        bool bufferTypeSelected = i == selectedOutputBufferType;
                        if (UISelectable(CSTR(outputBufferTypes[i]), bufferTypeSelected))
                            selectedOutputBufferType = i;
                        if (bufferTypeSelected) UISetItemFocusDefault();
                    }
                    UIEndCombo();
                }
                instance->graphics.outputBufferType = rawBufferTypes[selectedOutputBufferType];
            }
            UISeparator();
            UIText(CSTR(std::string(ELECTRON_GET_LOCALIZATION(instance, "RENDER_PREVIEW_PROFILING")) + ":"));
            for (int i = 0; i < instance->graphics.layers.size(); i++) {
                RenderLayer& layer = instance->graphics.layers[i];
                float renderTime = instance->graphics.layers[i].renderTime;
                UIText(CSTR(layer.layerPublicName + "<" + std::to_string(i) + ">: " + std::to_string(renderTime)));
            }
            UISpacing();
        UIEnd();

        ResolutionVariant currentResolution = resolutionVariants[selectedResolutionVariant];
        if (!resizeLerpEnabled) {
            if (currentResolution.raw.x != renderBuffer.width || currentResolution.raw.y != renderBuffer.height) {
                GraphicsImplResizeRenderBuffer(instance, currentResolution.raw.x, currentResolution.raw.y);
            }
        } else {
            if (beginInterpolation) {
                if (reiszeLerpPercentage >= 1.0f) {
                    beginInterpolation = false;
                    rebuildResolutions = false;
                    reiszeLerpPercentage = 0;
                } else {
                    ImVec2 interpolatedResolution = {ui_lerp(beginResizeResolution.x, targetResizeResolution.x, reiszeLerpPercentage), 
                                                     ui_lerp(beginResizeResolution.y, targetResizeResolution.y, reiszeLerpPercentage)};
                    GraphicsImplResizeRenderBuffer(instance, interpolatedResolution.x, interpolatedResolution.y);
                    reiszeLerpPercentage += 0.05f;
                }
            }
        }
        
        std::vector<int> projectResolution = JSON_AS_TYPE(instance->project.propertiesMap["ProjectResolution"], std::vector<int>);
        if ((resolutionVariants[0].width != projectResolution[0] || resolutionVariants[0].height != projectResolution[1]) && !beginInterpolation) {
            GraphicsImplResizeRenderBuffer(instance, projectResolution[0], projectResolution[1]);

            RebuildPreviewResolutions(resolutionVariants, {projectResolution[0], projectResolution[1]});

            ResolutionVariant selectedVariant = resolutionVariants[selectedResolutionVariant];
            GraphicsImplResizeRenderBuffer(instance, selectedVariant.width, selectedVariant.height);
        }
        internalFrameIndex++;
    }
}