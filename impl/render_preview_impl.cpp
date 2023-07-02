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

        UISetNextWindowSize(ElectronVector2f{640, 480}, ImGuiCond_Once);
        UIBegin(CSTR(ElectronImplTag(ELECTRON_GET_LOCALIZATION(instance, "RENDER_PREVIEW_WINDOW_TITLE"), owner)), ElectronSignal_CloseWindow, ImGuiWindowFlags_NoCollapse);
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

            ElectronVector2f scaledPreviewSize = {renderBuffer.width, renderBuffer.height};
            if (availZone.x * 2.0f < scaledPreviewSize.x) {
                scaledPreviewSize.x = availZone.x;
            } else {
                float scaleFactor = availZone.x / scaledPreviewSize.x * 0.3f;
                scaledPreviewSize.x += scaledPreviewSize.x * scaleFactor;
                scaledPreviewSize.y += scaledPreviewSize.y * scaleFactor;
            }
            scaledPreviewSize.x *= previewScale;
            scaledPreviewSize.y *= previewScale;


            GraphicsImplCleanPreviewGPUTexture(instance);
            GraphicsImplRequestRenderWithinRegion(instance, RenderRequestMetadata{0, renderBuffer.width, 0, renderBuffer.height, 60, JSON_AS_TYPE(instance->project.propertiesMap["BackgroundColor"], std::vector<float>)});
            PixelBufferImplSetFiltering(GL_NEAREST);
            GraphicsImplBuildPreviewGPUTexture(instance);
            GLuint gpuTex = GraphicsImplGetPreviewGPUTexture(instance);
            
            UISetCursorX(windowSize.x / 2.0f - scaledPreviewSize.x / 2.0f);
            UIImage(gpuTex, scaledPreviewSize);
            UISpacing();
            UISeparator();
            UISpacing();
            if (UICollapsingHeader(ELECTRON_GET_LOCALIZATION(instance, "RENDER_PREVIEW_SETTINGS_HEADER"))) {
                UISliderFloat(ELECTRON_GET_LOCALIZATION(instance, "RENDER_PREVIEW_PREVIEW_SCALE"), &previewScale, 0.5f, 2.0f, "%0.1f", 0);
                if (UIBeginCombo(ELECTRON_GET_LOCALIZATION(instance, "RENDER_PREVIEW_RESOLUTIONS_LABEL"), resolutionVariants[selectedResolutionVariant].repr.c_str())) {
                    for (int n = 0; n < 9; n++) {
                        bool resolutionSelected = (n == selectedResolutionVariant);
                        if (UISelectable(resolutionVariants[n].repr.c_str(), resolutionSelected)) {
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
        if (currentResolution.raw.x != renderBuffer.width || currentResolution.raw.y != renderBuffer.height) {
            GraphicsImplResizeRenderBuffer(instance, currentResolution.raw.x, currentResolution.raw.y);
        }
        print(currentResolution.width << " " << currentResolution.height);
    }
}