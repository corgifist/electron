#pragma once

#include "electron.h"
#include "time.h"
#include "libraries.h"
#include "async_ffmpeg.h"
#include "cache.h"
#include "shared.h"
#include "utils/gles2.h"
#include "ImGui/imgui.h"
#include "ImGui/imgui_stdlib.h"

#include "pixel_buffer.h"
#include "render_layer.h"
#include "driver_core.h"

#define MAX_DEPTH 100000000
#define IMPORT_EXTENSIONS ".png,.jpg,.jpeg,.tga,.psd,.ogg,.mp3,.wav,.*"

static DylibRegistry dylibRegistry{};

namespace Electron {

    enum PreviewOutputBufferType {
        PreviewOutputBufferType_Color,
        PreviewOutputBufferType_UV,
        PreviewOutputBufferType_Depth
    };

    class GraphicsCore;
    class RenderLayer;
    class AppCore;
    class RenderBuffer;

    static std::vector<float> fsQuadVertices = {
        -1.0f, 1.0f,
        -1.0f, -1.0f,
        1.0f, -1.0f,
        -1.0f, 1.0f,
        1.0f, -1.0f,
        1.0f, 1.0f
    };
    static std::vector<float> fsQuadUV = {
        0, 1,
        0, 0,
        1, 0,
        0, 1,
        1, 0,
        1, 1
    };

    // A set of GPU textures that represents color/depth/uv buffers
    class RenderBuffer {
    public:
        GLuint colorBuffer;
        GLuint uvBuffer;
        GLuint depthBuffer;
        int width, height;

        RenderBuffer(int width, int height);
        RenderBuffer() {
            this->width = 0;
            this->height = 0;
        };

        void Destroy();

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

    struct PipelineFrameBuffer {
        GLuint fbo, stencil;
        RenderBuffer rbo;
        int width, height;

        PipelineFrameBuffer(int width, int height);
        PipelineFrameBuffer() {}

        void Bind();
        void Unbind();
        void Destroy();
    };

    typedef int(*Electron_CacheIndexT)();
    
    // Responsible of some GPU manipulations
    struct GraphicsCore {
        static PipelineFrameBuffer renderBuffer;
        static PreviewOutputBufferType outputBufferType;
        static std::vector<RenderLayer> layers;
        static bool isPlaying;
        static std::unordered_map<GLuint, std::unordered_map<std::string, GLuint>> uniformCache;


        static float renderFrame;
        static int renderLength, renderFramerate;
        
        static void Initialize();

        static void AddRenderLayer(RenderLayer layer);
        static void FetchAllLayers();

        static DylibRegistry GetImplementationsRegistry();

        static void PrecompileEssentialShaders();

        static RenderLayer* GetLayerByID(int id);
        static int GetLayerIndexByID(int id);

        static void RequestRenderBufferCleaningWithinRegion();
        static void RequestTextureCollectionCleaning(PipelineFrameBuffer frb, float multiplier = 1.0f);
        static std::vector<float> RequestRenderWithinRegion();
        static void ResizeRenderBuffer(int width, int height);
        static GLuint GetPreviewGPUTexture();

        static GLuint GenerateVAO(std::vector<float> vertices, std::vector<float> uv);
        static GLuint CompileComputeShader(std::string path);
        static GLuint CompilePipelineShader(std::string path);
        static void UseShader(GLuint shader);
        static void DispatchComputeShader(int grid_x, int grid_y, int grid_z);
        static void ComputeMemoryBarier(GLbitfield barrier);
        static GLuint GenerateGPUTexture(int width, int height, int unit);
        static void BindGPUTexture(GLuint texture, GLuint shader, int unit, std::string uniform);
        static void ShaderSetUniform(GLuint program, std::string name, int x, int y);
        static void ShaderSetUniform(GLuint program, std::string name, glm::vec3 vec);
        static void ShaderSetUniform(GLuint program, std::string name, float f);
        static void ShaderSetUniform(GLuint program, std::string name, glm::vec2 vec);
        static void ShaderSetUniform(GLuint program, std::string name, int x);
        static void ShaderSetUniform(GLuint program, std::string name, glm::vec4 vec);
        static void ShaderSetUniform(GLuint program, std::string name, glm::mat3 mat3);
        static void ShaderSetUniform(GLuint program, std::string name, glm::mat4 mat4);

        static GLuint GetUniformLocation(GLuint program, std::string name);

        static void FireTimelineSeek();
        static void FirePlaybackChange();

        static void CallCompositor(PipelineFrameBuffer frb);
        
    };
}