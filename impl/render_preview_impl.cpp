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

    void RebuildPreviewResolutions(ResolutionVariant* resolutionVariants, PixelBuffer& renderBuffer) {
        ImVec2 fractSize{renderBuffer.width / 10.0f, renderBuffer.height / 10.0f};
        ImVec2 sizeAcc{renderBuffer.width, renderBuffer.height};

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

        bool resizeLerpEnabled = JSON_AS_TYPE(instance->configMap["ResizeInterpolation"], bool);

        UISetNextWindowSize(ElectronVector2f{640, 480}, ImGuiCond_Once);
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
            PixelBuffer& renderBuffer = instance->graphics.renderBuffer;

            if (firstSetup) {
                RebuildPreviewResolutions(resolutionVariants, renderBuffer);
                firstSetup = false;
            }

            ElectronVector2f scaledPreviewSize = {resolutionVariants[0].width, resolutionVariants[0].height};

            float scaleFactor = ((availZone.x / scaledPreviewSize.x * 0.1f) + (availZone.y / scaledPreviewSize.y * 0.1f)) / 2.0f;
            scaledPreviewSize.x += scaledPreviewSize.x * scaleFactor;
            scaledPreviewSize.y += scaledPreviewSize.y * scaleFactor;
            scaledPreviewSize.x *= previewScale;
            scaledPreviewSize.y *= previewScale;


            GraphicsImplCleanPreviewGPUTexture(instance);

            instance->graphics.renderLength = 60;
            instance->graphics.renderFramerate = 30;
            GraphicsImplRequestRenderWithinRegion(instance, RenderRequestMetadata{0, renderBuffer.width, 0, renderBuffer.height, JSON_AS_TYPE(instance->project.propertiesMap["BackgroundColor"], std::vector<float>)});
            GraphicsImplBuildPreviewGPUTexture(instance);
            GLuint gpuTex = GraphicsImplGetPreviewGPUTexture(instance);
            
            UISetCursorX(windowSize.x / 2.0f - scaledPreviewSize.x / 2.0f);
            UIImage(gpuTex, scaledPreviewSize);
            float translatedTimelineValue = (float) instance->graphics.renderFrame / (float) instance->graphics.renderFramerate;
            UIPushItemWidth(windowSize.x * 0.95f);
                UISliderFloat("##", &translatedTimelineValue, 0, (float) instance->graphics.renderLength / (float) instance->graphics.renderFramerate, CSTR(std::string("%0.") + std::to_string(JSON_AS_TYPE(instance->configMap["RenderPreviewTimelinePrescision"], int)) + "f"), 0);
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
            }
            UISeparator();
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
                    reiszeLerpPercentage = 0;
                } else {
                    ImVec2 interpolatedResolution = {lerp(beginResizeResolution.x, targetResizeResolution.x, reiszeLerpPercentage), 
                                                     lerp(beginResizeResolution.y, targetResizeResolution.y, reiszeLerpPercentage)};
                    print(interpolatedResolution.x << " " << interpolatedResolution.y);
                    GraphicsImplResizeRenderBuffer(instance, interpolatedResolution.x, interpolatedResolution.y);
                    reiszeLerpPercentage += 0.05f;
                }
            }
        }
        
        std::vector<int> projectResolution = JSON_AS_TYPE(instance->project.propertiesMap["ProjectResolution"], std::vector<int>);
        if (resolutionVariants[0].width != projectResolution[0] || resolutionVariants[0].height != projectResolution[1]) {
            GraphicsImplResizeRenderBuffer(instance, projectResolution[0], projectResolution[1]);

            RebuildPreviewResolutions(resolutionVariants, renderBuffer);
        }
        internalFrameIndex++;
    }
}