#pragma once

#include "electron.h"

namespace Electron {
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

        int frame;

        std::vector<float> backgroundColor;

        RenderRequestMetadata() {}

        RenderRequestMetadata(int beginX, int endX, int beginY, int endY, int frame, std::vector<float> backgroundColor) {
            this->beginX = beginX;
            this->endX = endX;
            this->beginY = beginY;
            this->endY = endY;
            this->frame = frame;

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

    class GraphicsCore {
    public:
        PixelBuffer renderBuffer;
        GLuint previousRenderBufferTexture;
        GLuint renderBufferTexture;
        
        GraphicsCore();

        void RequestRenderWithinRegion(RenderRequestMetadata metadata);
        void ResizeRenderBuffer(int width, int height);
        void CleanPreviewGPUTexture();
        void BuildPreviewGPUTexture();

        PixelBuffer CreateBufferFromImage(const char* filename);
    };
}