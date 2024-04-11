#include "graphics_core.h"

namespace Electron {

    PreviewOutputBufferType GraphicsCore::outputBufferType;
    std::vector<RenderLayer> GraphicsCore::layers;
    bool GraphicsCore::isPlaying;
    float GraphicsCore::renderFrame;
    int GraphicsCore::renderLength, GraphicsCore::renderFramerate;
    std::vector<PipelineFrameBuffer> GraphicsCore::compositorQueue;

    void GraphicsCore::Initialize() {

        GraphicsCore::renderFrame = 0;
        GraphicsCore::renderFramerate = 60; // default render framerate
        GraphicsCore::renderLength = 0;

        GraphicsCore::outputBufferType = PreviewOutputBufferType_Color;
    }

    void GraphicsCore::Destroy() {
        for (auto& layer : layers) {
            layer.Destroy();
        }
        layers.clear();
    }

    void GraphicsCore::FetchAllLayers() {
        auto iterator = std::filesystem::directory_iterator("layers");
        for (auto &entry : iterator) {
            std::string transformedPath = std::regex_replace(
                base_name(entry.path().string()), std::regex(".dll|.so|lib"), "");
            Libraries::LoadLibrary("layers", transformedPath);
            print("Pre-render fetching " << transformedPath);
            dylibRegistry.push_back(transformedPath);
        }
        print("Fetched all layers");
    }

    DylibRegistry GraphicsCore::GetImplementationsRegistry() {
        return dylibRegistry;
    }

    RenderLayer *GraphicsCore::GetLayerByID(int id) {
        for (auto &layer : layers) {
            if (layer.id == id)
                return &layer;
        }

        throw std::runtime_error(string_format("cannot find layer with id %i", id));
    }

    int GraphicsCore::GetLayerIndexByID(int id) {
        int index = 0;
        for (auto &layer : layers) {
            if (layer.id == id)
                return index;
            index++;
        }

        return -1;
    }

    void GraphicsCore::AddRenderLayer(RenderLayer layer) {
        layers.push_back(layer);
    }

    void GraphicsCore::RequestTextureCollectionCleaning(GPUExtendedHandle context, PipelineFrameBuffer frb,
                                                        float multiplier) {
        glm::vec4 backgroundColor = {
                Shared::backgroundColor, multiplier
        };
        float blackPtr[] = {
            0.0f, 0.0f, 0.0f, multiplier
        };
        DriverCore::ClearTextureImage(context, frb.rbo.colorBuffer, 0, glm::value_ptr(backgroundColor));
        DriverCore::ClearTextureImage(context, frb.rbo.uvBuffer, 0, blackPtr);
    }

    void GraphicsCore::DrawArrays(GPUExtendedHandle context, int size) {
        Shared::renderCalls++;
        DriverCore::DrawArrays(context, size);
    }

    GPUExtendedHandle GraphicsCore::CompileComputeShader(std::string path) {
        // return DriverCore::GenerateShaderProgram(ShaderType::Compute, string_format("%s\n layout(local_size_x = %i, local_size_y = %i, local_size_z = %i) in;\n%s", Shared::glslVersion.c_str(), Wavefront::x, Wavefront::y, Wavefront::z, read_file("compute/" + path).c_str()).c_str());
        return 0;
    }

    PipelineShader GraphicsCore::CompilePipelineShader(std::string path, ShaderType type) {
        PipelineShader shader;
        path = "compute/spirv/" + path;

        if (type == ShaderType::Vertex || type == ShaderType::VertexFragment) {
            shader.vertex = DriverCore::GenerateShaderProgram(ShaderType::Vertex, read_bytes(path + ".vert.spv"));
            if (!shader.vertex) throw std::runtime_error("cannot compile pipeline vertex shader " + path);
        }

        if (type == ShaderType::Fragment || type == ShaderType::VertexFragment) {
            shader.fragment = DriverCore::GenerateShaderProgram(ShaderType::Fragment, read_bytes(path + ".frag.spv"));
            if (!shader.fragment) throw std::runtime_error("cannot compile pipeline fragment shader " + path);
        }

        return shader;
    }

    void GraphicsCore::UseShader(ShaderType stage, GPUExtendedHandle shader) {
    }

    GPUExtendedHandle GraphicsCore::GenerateGPUTexture(int width, int height) {
        return VRAM::GenerateGPUTexture(0, width, height);
    }

    void GraphicsCore::DispatchComputeShader(int grid_x, int grid_y, int grid_z) {
        DriverCore::DispatchCompute(std::ceil(grid_x / Wavefront::x),
                        std::ceil(grid_y / Wavefront::y),
                        std::ceil(grid_z / Wavefront::z));
    }

    void GraphicsCore::ComputeMemoryBarier(MemoryBarrierType barrier) {
        DriverCore::MemoryBarrier(barrier);
    }

    void GraphicsCore::CallCompositor(PipelineFrameBuffer frb) {
        compositorQueue.push_back(frb);
    }

    void GraphicsCore::FireTimelineSeek() {
        Shared::timelineSeekFired = true;
        for (auto &layer : layers) {
            layer.onTimelineSeek(&layer);
        }
    }

    void GraphicsCore::FirePlaybackChange() {
        for (auto &layer : layers) {
            layer.onPlaybackChangeProcedure(&layer);
        }
    }

} // namespace Electron