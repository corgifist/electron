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

    struct CompositorPushConstants {
        glm::vec2 viewport;
    };

    // Responsible of some GPU manipulations
    struct GraphicsCore {
        static PreviewOutputBufferType outputBufferType;
        static std::vector<RenderLayer> layers;
        static bool isPlaying;
        static std::vector<PipelineFrameBuffer> compositorQueue;

        static float renderFrame;
        static int renderLength, renderFramerate;
        
        static void Initialize();
        static void Destroy();

        static void AddRenderLayer(RenderLayer layer);
        static void FetchAllLayers();

        static DylibRegistry GetImplementationsRegistry();

        static RenderLayer* GetLayerByID(int id);
        static int GetLayerIndexByID(int id);

        static void RequestRenderBufferCleaningWithinRegion(GPUExtendedHandle context);
        static void RequestTextureCollectionCleaning(GPUExtendedHandle context, PipelineFrameBuffer frb, float multiplier = 1.0f);
        static GLuint GetPreviewGPUTexture();

        static void DrawArrays(GPUExtendedHandle context, int size);
        static GPUExtendedHandle CompileComputeShader(std::string path);
        static PipelineShader CompilePipelineShader(std::string path, ShaderType type = ShaderType::VertexFragment);
        static void UseShader(ShaderType type, GPUExtendedHandle shader);
        static void DispatchComputeShader(int grid_x, int grid_y, int grid_z);
        static void ComputeMemoryBarier(MemoryBarrierType barrier);
        static GPUExtendedHandle GenerateGPUTexture(int width, int height);

        static void FireTimelineSeek();
        static void FirePlaybackChange();

        static void CallCompositor(PipelineFrameBuffer frb);
        static void PerformComposition(GPUExtendedHandle context);
        
    };
}