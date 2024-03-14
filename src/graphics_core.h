#pragma once

#include "electron.h"
#include "time.h"
#include "libraries.h"
#include "async_ffmpeg.h"
#include "cache.h"
#include "shared.h"
#include "ImGui/imgui.h"
#include "ImGui/imgui_stdlib.h"

#include "pixel_buffer.h"
#include "render_layer.h"
#include "driver_core.h"

#include "vram.h"

static DylibRegistry dylibRegistry{};

namespace Electron {

    enum PreviewOutputBufferType {
        PreviewOutputBufferType_Color,
        PreviewOutputBufferType_UV
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
        GPUExtendedHandle vertex, fragment, compute;
        PipelineShader() {
            this->vertex = 0;
            this->fragment = 0;
            this->compute = 0;
        }
    };
    
    // Responsible of some GPU manipulations
    struct GraphicsCore {
        static PipelineFrameBuffer renderBuffer;
        static PreviewOutputBufferType outputBufferType;
        static std::vector<RenderLayer> layers;
        static bool isPlaying;
        static std::unordered_map<GPUExtendedHandle, std::unordered_map<std::string, GPUExtendedHandle>> uniformCache;
        static std::vector<PipelineFrameBuffer> compositorQueue;

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

        static GPUExtendedHandle GenerateVAO(std::vector<float> vertices, std::vector<float> uv);
        static void DrawArrays(GLuint vao, int size);
        static GPUExtendedHandle CompileComputeShader(std::string path);
        static PipelineShader CompilePipelineShader(std::string path, ShaderType type = ShaderType::VertexFragment);
        static void UseShader(ShaderType type, GPUExtendedHandle shader);
        static void DispatchComputeShader(int grid_x, int grid_y, int grid_z);
        static void ComputeMemoryBarier(MemoryBarrierType barrier);
        static GPUExtendedHandle GenerateGPUTexture(int width, int height);

        static GPUExtendedHandle GetUniformLocation(GPUExtendedHandle program, std::string name);

        static void FireTimelineSeek();
        static void FirePlaybackChange();

        static void CallCompositor(PipelineFrameBuffer frb);
        static void PerformForwardComposition();
        static void PerformComposition();
        
    };
}