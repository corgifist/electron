#pragma once

#include "electron.h"
#include "time.h"
#include "libraries.h"
#include "async_ffmpeg.h"
#include "cache.h"
#include "shared.h"
#include "gles2.h"
#include "ImGui/imgui.h"
#include "ImGui/imgui_stdlib.h"

#include "pixel_buffer.h"
#include "render_layer.h"

#define MAX_DEPTH 100000000
#define IMPORT_EXTENSIONS ".png,.jpg,.jpeg,.tga,.psd,.ogg,.mp3,.wav,.*"

#define RENDER_THREADS_MULTIPLIER 1

static DylibRegistry dylibRegistry{};

namespace Electron {

    enum PreviewOutputBufferType {
        PreviewOutputBufferType_Color,
        PreviewOutputBufferType_UV,
        PreviewOutputBufferType_Depth
    };

    class GraphicsCore;
    class RenderLayer;
    class AppInstance;
    class RenderBuffer;

    // A set of GPU textures that represents color/depth/uv buffers
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

    // GPUTexture that adapts to preview buffer size changes
    struct ResizableGPUTexture {
        GLuint texture;
        int width, height;

        ResizableGPUTexture(int width, int height);
        ResizableGPUTexture() {}

        void CheckForResize(RenderBuffer* rbo);
        void Destroy();
    };

    // RenderBuffer that adapts to preview buffer size changes
    struct ResizableRenderBuffer {
        ResizableGPUTexture color, uv, depth;

        ResizableRenderBuffer(int width, int height);
        ResizableRenderBuffer() {}

        void CheckForResize(RenderBuffer* rbo);
        void Destroy();
    };

    typedef int(*Electron_CacheIndexT)();
    
    // Responsible of some GPU manipulations
    struct GraphicsCore {
        RenderBuffer renderBuffer;
        PreviewOutputBufferType outputBufferType;
        std::vector<RenderLayer> layers;
        bool isPlaying;


        float renderFrame;
        int renderLength, renderFramerate;
        
        GraphicsCore();

        void AddRenderLayer(RenderLayer layer);
        void FetchAllLayers();

        static DylibRegistry GetImplementationsRegistry();

        void PrecompileEssentialShaders();

        RenderLayer* GetLayerByID(int id);
        int GetLayerIndexByID(int id);

        void RequestRenderBufferCleaningWithinRegion();
        void RequestTextureCollectionCleaning(GLuint color, GLuint uv, GLuint depth, int width, int height);
        std::vector<float> RequestRenderWithinRegion();
        void ResizeRenderBuffer(int width, int height);
        GLuint GetPreviewGPUTexture();

        static GLuint CompileComputeShader(std::string path);
        static void UseShader(GLuint shader);
        static void DispatchComputeShader(int grid_x, int grid_y, int grid_z);
        static void ComputeMemoryBarier(GLbitfield barrier);
        static GLuint GenerateGPUTexture(int width, int height, int unit);
        static void BindGPUTexture(GLuint texture, int unit, int readStatus);
        static void ShaderSetUniform(GLuint program, std::string name, int x, int y);
        static void ShaderSetUniform(GLuint program, std::string name, glm::vec3 vec);
        static void ShaderSetUniform(GLuint program, std::string name, float f);
        static void ShaderSetUniform(GLuint program, std::string name, glm::vec2 vec);
        static void ShaderSetUniform(GLuint program, std::string name, int x);
        static void ShaderSetUniform(GLuint program, std::string name, glm::vec4 vec);

        void FireTimelineSeek();
        void FirePlaybackChange();

        void CallCompositor(ResizableGPUTexture color, ResizableGPUTexture uv, ResizableGPUTexture depth);
    };
}