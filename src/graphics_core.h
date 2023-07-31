#pragma once

#include "electron.h"
#include "dylib.hpp"
#include "time.h"
#include "ImGui/imgui.h"

#define MAX_DEPTH 100000000

namespace Electron {

    enum PreviewOutputBufferType {
        PreviewOutputBufferType_Color,
        PreviewOutputBufferType_UV,
        PreviewOutputBufferType_Depth
    };

    enum class GeneralizedPropertyType {
        Vec3, Vec2, Float
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
        int beginX, endX, beginY, endY;
        std::vector<float> backgroundColor;

        RenderRequestMetadata() {}
    };

    enum class TextureUnionType {
        Texture
    };

    class PixelBuffer {
    private:
        std::vector<Pixel> pixels;

    public:
        int width, height;
        static int filtering;

        PixelBuffer(int width, int height);
        PixelBuffer(std::vector<Pixel> pixels, int width, int height);
        PixelBuffer() {};

        void SetPixel(int x, int y, Pixel);
        Pixel GetPixel(int x, int y);

        void FillColor(Pixel pixel, RenderRequestMetadata metadata);

        GLuint BuildGPUTexture();
    };

    using InternalTextureUnion = std::variant<PixelBuffer>;

    struct TextureUnion {
        TextureUnionType type;
        InternalTextureUnion as;
        std::string name;
        std::string path;

        TextureUnion() {}
        ~TextureUnion() {}
    }; 

    class RenderBuffer {
    public:
        PixelBuffer color;
        PixelBuffer uv;
        PixelBuffer depth;

        RenderBuffer(int width, int height);
        RenderBuffer() = default;
    };

    struct AssetRegistry {
        std::vector<TextureUnion> assets;
        GraphicsCore* owner;

        AssetRegistry() {}

        void LoadFromProject(json_t project);
        void Clear();

        static TextureUnionType TextureUnionTypeFromString(std::string type) {
            if (type == "Image") {
                return TextureUnionType::Texture;
            }
            return TextureUnionType::Texture;
        }
    };

    typedef void (*Electron_LayerImplF)(RenderLayer*, RenderRequestMetadata);
    typedef void (*Electron_PropertyRenderImplF)(RenderLayer*);


    class RenderLayer {
    private:
        dylib layerImplementation;
    public:
        int beginFrame, endFrame, frameOffset;
        std::string layerLibrary;
        json_t properties;
        Electron_LayerImplF layerProcedure;
        Electron_PropertyRenderImplF initializationProcedure;
        Electron_PropertyRenderImplF propertiesProcedure;
        Electron_PropertyRenderImplF sortingProcedure;
        GraphicsCore* graphicsOwner;
        bool initialized;
        std::string layerPublicName;
        float renderTime;


        RenderLayer(std::string layerLibrary); 
        RenderLayer() {};

        ~RenderLayer() {};

        void Render(GraphicsCore* graphics, RenderRequestMetadata metadata);
        void RenderProperty(GeneralizedPropertyType type, json_t& property, std::string propertyName);
        void RenderProperties();

        void SortKeyframes(json_t& keyframes);
        json_t InterpolateProperty(json_t property);
        json_t ExtractExactValue(json_t property);
    };

    class GraphicsCore {
    public:
        RenderBuffer renderBuffer;
        PreviewOutputBufferType outputBufferType;
        GLuint previousRenderBufferTexture;
        GLuint renderBufferTexture;
        std::vector<RenderLayer> layers;

        float renderFrame;
        int renderLength, renderFramerate;
        
        GraphicsCore();

        void RequestRenderWithinRegion(RenderRequestMetadata metadata);
        void ResizeRenderBuffer(int width, int height);
        void CleanPreviewGPUTexture();
        void BuildPreviewGPUTexture();
        PixelBuffer& GetPreviewBufferByOutputType();

        PixelBuffer CreateBufferFromImage(const char* filename);
    };
}