#pragma once

#include "electron.h"
#include "dylib.hpp"

namespace Electron {

    enum PreviewOutputBufferType {
        PreviewOutputBufferType_Color,
        PreviewOutputBufferType_UV,
        PreviewOutputBufferType_Depth
    };

    class GraphicsCore;
    class RenderLayer;

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
        std::vector<float> backgroundColor;

        RenderRequestMetadata() {}
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

    typedef void (*Electron_LayerImplF)(RenderLayer*);


    class RenderLayer {
    public:
        int beginFrame, endFrame, frameOffset;
        dylib layerImplementation;
        std::string layerLibrary;
        json_t properties;
        Electron_LayerImplF layerProcedure;
        GraphicsCore* graphicsOwner;
        bool initialized;

        RenderLayer(std::string layerLibrary); 
        RenderLayer() = default;

        void Render(GraphicsCore* graphics);
    };

    class GraphicsCore {
    public:
        RenderBuffer renderBuffer;
        PreviewOutputBufferType outputBufferType;
        GLuint previousRenderBufferTexture;
        GLuint renderBufferTexture;
        std::vector<RenderLayer> layers;

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