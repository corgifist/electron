#pragma once

#include "electron.h"

namespace Electron {

    enum PreviewOutputBufferType {
        PreviewOutputBufferType_Color,
        PreviewOutputBufferType_UV,
        PreviewOutputBufferType_Depth
    };

    struct Pixel {
        float r, g, b, a;

        Pixel(float r, float g, float b, float a) {
            this->r = r;
            this->g = g;
            this->b = b;
            this->a = a;
        };

        Pixel() = default;
    };

    struct RenderRequestMetadata {
        int beginX, endX;
        int beginY, endY;

        std::vector<float> backgroundColor;

        RenderRequestMetadata() {}

        RenderRequestMetadata(int beginX, int endX, int beginY, int endY, std::vector<float> backgroundColor) {
            this->beginX = beginX;
            this->endX = endX;
            this->beginY = beginY;
            this->endY = endY;

            this->backgroundColor = backgroundColor;
        }
    };

    class PixelBuffer {
    private:
        std::vector<Pixel> pixels;

    public:
        int width, height;
        static int filtering;

        PixelBuffer(int width, int height);
        PixelBuffer(std::vector<Pixel> pixels, int width, int height);
        PixelBuffer() = default;

        void SetPixel(int x, int y, Pixel);
        Pixel GetPixel(int x, int y);

        void FillColor(Pixel pixel);

        GLuint BuildGPUTexture();
    };

    class RenderBuffer {
    public:
        PixelBuffer color;
        PixelBuffer uv;
        PixelBuffer depth;

        RenderBuffer(int width, int height);
        RenderBuffer() = default;
    };

    class GraphicsCore {
    public:
        RenderBuffer renderBuffer;
        PreviewOutputBufferType outputBufferType;
        GLuint previousRenderBufferTexture;
        GLuint renderBufferTexture;

        int renderFrame, renderLength, renderFramerate;
        
        GraphicsCore();

        void RequestRenderWithinRegion(RenderRequestMetadata metadata);
        void ResizeRenderBuffer(int width, int height);
        void CleanPreviewGPUTexture();
        void BuildPreviewGPUTexture();
        PixelBuffer& GetPreviewBufferByOutputType();

        PixelBuffer CreateBufferFromImage(const char* filename);
    };
}