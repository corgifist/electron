#pragma once

#include "electron.h"
#include "time.h"
#include "libraries.h"
#include "async_ffmpeg.h"
#include "cache.h"
#include "shared.h"
#include "utils/gl.h"
#include "ImGui/imgui.h"
#include "ImGui/imgui_stdlib.h"

#include "pixel_buffer.h"
#include "render_layer.h"
#include "driver_core.h"

#include "vram.h"

#define MAX_DEPTH 100000000
#define IMPORT_EXTENSIONS ".png,.jpg,.jpeg,.tga,.psd,.ogg,.mp3,.wav,.*"

#define SAMPLER_BUFFER_SIZE 1024


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

    struct PipelineShader {
        GLuint vertex, fragment, compute;
        PipelineShader() {
            this->vertex = 0;
            this->fragment = 0;
            this->compute = 0;
        }
    };

    enum class ShaderType {
        Vertex, Fragment, Compute, VertexFragment
    };
    
    // Responsible of some GPU manipulations
    struct GraphicsCore {
        static PipelineFrameBuffer renderBuffer;
        static PreviewOutputBufferType outputBufferType;
        static std::vector<RenderLayer> layers;
        static bool isPlaying;
        static std::unordered_map<GLuint, std::unordered_map<std::string, GLuint>> uniformCache;
        static std::vector<PipelineFrameBuffer> compositorQueue;
        static GLuint pipeline;

        static PipelineShader basic, compositor, channel;

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
        static void DrawArrays(GLuint vao, int size);
        static GLuint CompileComputeShader(std::string path);
        static PipelineShader CompilePipelineShader(std::string path, ShaderType type = ShaderType::VertexFragment);
        static void UseShader(GLenum bitfield, GLuint shader);
        static void DispatchComputeShader(int grid_x, int grid_y, int grid_z);
        static void ComputeMemoryBarier(GLbitfield barrier);
        static GLuint GenerateGPUTexture(int width, int height);
        static void BindGPUTexture(GLuint texture, GLuint shader, int unit, std::string uniform);
        static void BindComputeGPUTexture(GLuint texture, GLuint unit, GLuint readStatus);
        static void ShaderSetUniform(GLuint program, std::string name, int x, int y);
        static void ShaderSetUniform(GLuint program, std::string name, glm::vec3 vec);
        static void ShaderSetUniform(GLuint program, std::string name, float f);
        static void ShaderSetUniform(GLuint program, std::string name, glm::vec2 vec);
        static void ShaderSetUniform(GLuint program, std::string name, int x);
        static void ShaderSetUniform(GLuint program, std::string name, glm::vec4 vec);
        static void ShaderSetUniform(GLuint program, std::string name, glm::mat3 mat3);
        static void ShaderSetUniform(GLuint program, std::string name, glm::mat4 mat4);
        static void ShaderSetUniform(GLuint program, std::string name, bool b);

        static GLuint GetUniformLocation(GLuint program, std::string name);

        static void FireTimelineSeek();
        static void FirePlaybackChange();

        static void CallCompositor(PipelineFrameBuffer frb);
        static void PerformComposition();
        
    };
}