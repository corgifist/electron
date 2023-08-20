#pragma once

#include "electron.h"
#include "dylib.hpp"
#include "time.h"
#include "ImGui/imgui.h"
#include "GLEW/include/GL/glew.h"

#define MAX_DEPTH 100000000
#define IMPORT_EXTENSIONS ".png,.jpg,.jpeg,.tga,.psd,.*"

#define RENDER_THREADS_MULTIPLIER 1

namespace Electron {

    enum PreviewOutputBufferType {
        PreviewOutputBufferType_Color,
        PreviewOutputBufferType_UV,
        PreviewOutputBufferType_Depth
    };

    enum class GeneralizedPropertyType {
        Vec3, Vec2, Float, Color3
    };

    class GraphicsCore;
    class RenderLayer;
    class AppInstance;
    class RenderBuffer;

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
        RenderBuffer* rbo;
        int offsetX, offsetY;

        RenderRequestMetadata() {
            this->offsetX = 0;
            this->offsetY = 0;
        }
    };

    enum class TextureUnionType {
        Texture
    };

    class PixelBuffer {
    public:
        int width, height;
        static int filtering;
        std::vector<Pixel> pixels;

        PixelBuffer(int width, int height);
        PixelBuffer(std::vector<Pixel> pixels, int width, int height);
        PixelBuffer() {};

        void SetPixel(int x, int y, Pixel);
        Pixel GetPixel(int x, int y);

        void FillColor(Pixel pixel, RenderRequestMetadata metadata);

        GLuint BuildGPUTexture();

        static void DestroyGPUTexture(GLuint texture);
    };

    using InternalTextureUnion = std::variant<PixelBuffer>;
    struct AssetRegistry;
    struct TextureUnion;

    struct TextureUnion {
        TextureUnionType type;
        InternalTextureUnion as;
        std::string name;
        std::string path;
        std::string strType;
        GLuint pboGpuTexture;
        GLuint previousPboGpuTexture;
        bool invalid;
        float previewScale;
        AssetRegistry* assetOwner;
        int id;

        TextureUnion() {
            this->previewScale = 1.0f;
            this->invalid = false;
        }
        ~TextureUnion() {
        }

        bool IsTextureCompatible() {
            return type == TextureUnionType::Texture;
        }

        void RebuildAssetData(GraphicsCore* owner);

        glm::vec2 GetDimensions();

        void GenerateUVTexture();
    }; 

    struct AssetLoadInfo {
        TextureUnion result;
        std::string returnMessage;

        AssetLoadInfo(std::string msg) {
            this->returnMessage = msg;
        }

        AssetLoadInfo(const char* msg) {
            this->returnMessage = msg;
        }

        AssetLoadInfo() {}
    };

    class RenderBuffer {
    public:
        GLuint colorBuffer;
        GLuint uvBuffer;
        GLuint depthBuffer;
        int width, height;

        RenderBuffer(GraphicsCore* core, int width, int height);
        RenderBuffer() {
            this->width = 0;
            this->height = 0;
        };

        ~RenderBuffer();
    };

    struct AssetRegistry {
        std::vector<TextureUnion> assets;
        GraphicsCore* owner;

        AssetRegistry() {}

        void LoadFromProject(json_t project);
        void Clear();

        std::string ImportAsset(std::string path);
        AssetLoadInfo LoadAssetFromPath(std::string path);

        static TextureUnionType TextureUnionTypeFromString(std::string type) {
            if (type == "Image") {
                return TextureUnionType::Texture;
            }
            return TextureUnionType::Texture;
        }

        static std::string StringFromTextureUnionType(TextureUnionType type) {
            switch (type) {
                case TextureUnionType::Texture: {
                    return "Image";
                }
            }
            return "Image";
        }
    };

    typedef void (*Electron_LayerImplF)(RenderLayer*, RenderRequestMetadata);
    typedef void (*Electron_PropertyRenderImplF)(RenderLayer*);


    class RenderLayer {
    public:
        dylib layerImplementation;
        int beginFrame, endFrame, frameOffset;
        std::string layerLibrary;
        json_t properties;
        json_t internalData;
        Electron_LayerImplF layerProcedure;
        Electron_PropertyRenderImplF initializationProcedure;
        Electron_PropertyRenderImplF propertiesProcedure;
        Electron_PropertyRenderImplF sortingProcedure;
        GraphicsCore* graphicsOwner;
        bool initialized;
        std::string layerPublicName;
        float renderTime;
        std::vector<std::any> anyData;
        glm::vec4 layerColor;
        int registryIndex;


        RenderLayer(std::string layerLibrary); 
        RenderLayer() {};

        ~RenderLayer();

        void FetchImplementation();

        void Render(GraphicsCore* graphics, RenderRequestMetadata metadata);
        void RenderProperty(GeneralizedPropertyType type, json_t& property, std::string propertyName);
        void RenderProperties();

        void SortKeyframes(json_t& keyframes);
        json_t InterpolateProperty(json_t property);
        json_t ExtractExactValue(json_t property);
    };

    struct RenderLayerRegistry {
        static std::vector<RenderLayer*> Registry;
    };

    struct ResizableGPUTexture {
        GLuint texture;
        int width, height;
        GraphicsCore* core;

        ResizableGPUTexture(GraphicsCore* core, int width, int height);
        ResizableGPUTexture() {}

        void CheckForResize(RenderBuffer* rbo);
    };

    class GraphicsCore {
    public:
        RenderBuffer renderBuffer;
        PreviewOutputBufferType outputBufferType;
        GLuint previousRenderBufferTexture;
        GLuint renderBufferTexture;
        std::vector<RenderLayer> layers;
        AppInstance* owner;
        std::string projectPath;

        float renderFrame;
        int renderLength, renderFramerate;
        
        GraphicsCore();

        void RequestRenderBufferCleaningWithinRegion(RenderRequestMetadata metadata);
        void RequestTextureCollectionCleaning(GLuint color, GLuint uv, GLuint depth, int width, int height, RenderRequestMetadata metadata);
        std::vector<float> RequestRenderWithinRegion(RenderRequestMetadata metadata);
        void ResizeRenderBuffer(int width, int height);
        void CleanPreviewGPUTexture();
        void BuildPreviewGPUTexture();
        GLuint GetPreviewBufferByOutputType();

        GLuint CompileComputeShader(std::string path);
        void UseShader(GLuint shader);
        void DispatchComputeShader(int grid_x, int grid_y, int grid_z);
        void ComputeMemoryBarier(GLbitfield barrier);
        GLuint GenerateGPUTexture(int width, int height, int unit);
        void BindGPUTexture(GLuint texture, int unit);
        PixelBuffer PBOFromGPUTexture(GLuint texture, int width, int height);
        void ShaderSetUniform(GLuint program, std::string name, int x, int y);
        void ShaderSetUniform(GLuint program, std::string name, glm::vec3 vec);
        void ShaderSetUniform(GLuint program, std::string name, float f);
        void ShaderSetUniform(GLuint program, std::string name, glm::vec2 vec);
        void ShaderSetUniform(GLuint program, std::string name, int x);

        void CallCompositor(ResizableGPUTexture color, ResizableGPUTexture uv, ResizableGPUTexture depth);

        PixelBuffer CreateBufferFromImage(const char* filename);

    };
}