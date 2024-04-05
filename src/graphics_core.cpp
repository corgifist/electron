#include "graphics_core.h"

namespace Electron {

    PipelineFrameBuffer GraphicsCore::renderBuffer;
    PreviewOutputBufferType GraphicsCore::outputBufferType;
    std::vector<RenderLayer> GraphicsCore::layers;
    bool GraphicsCore::isPlaying;
    float GraphicsCore::renderFrame;
    int GraphicsCore::renderLength, GraphicsCore::renderFramerate;
    std::vector<PipelineFrameBuffer> GraphicsCore::compositorQueue;
    GraphicsCore::CompositorPipeline GraphicsCore::compositorPipeline;



    void GraphicsCore::Initialize() {

        GraphicsCore::renderFrame = 0;
        GraphicsCore::renderFramerate = 60; // default render framerate
        GraphicsCore::renderLength = 0;

        GraphicsCore::outputBufferType = PreviewOutputBufferType_Color;
    }

    void GraphicsCore::Destroy() {
        renderBuffer.Destroy();
        for (auto& layer : layers) {
            layer.Destroy();
        }
        layers.clear();
    }

    void GraphicsCore::PrecompileEssentialShaders() {
        auto compositorShader = CompilePipelineShader("compositor");

        DescriptorSetLayoutBinding compositorQueueBindingColor;
        compositorQueueBindingColor.bindingPoint = 0;
        compositorQueueBindingColor.descriptorType = DescriptorType::Sampler;
        compositorQueueBindingColor.shaderType = ShaderType::Fragment;

        DescriptorSetLayoutBinding compositorQueueBindingUV;
        compositorQueueBindingUV.bindingPoint = 1;
        compositorQueueBindingUV.descriptorType = DescriptorType::Sampler;
        compositorQueueBindingUV.shaderType = ShaderType::Fragment;

        DescriptorSetLayoutBuilder compositorQueueSetBuilder;
        compositorQueueSetBuilder.AddBinding(compositorQueueBindingColor);
        compositorQueueSetBuilder.AddBinding(compositorQueueBindingUV);

        GPUExtendedHandle builtCompositorQueueSetLayout = compositorQueueSetBuilder.Build();

        RenderPipelinePushConstantRange compositorQueuePushConstantRange;
        compositorQueuePushConstantRange.offset = 0;
        compositorQueuePushConstantRange.size = sizeof(CompositorPushConstants);
        compositorQueuePushConstantRange.shaderStage = ShaderType::Fragment;

        GPUExtendedHandle builtCompositorQueuePushConstantRange = compositorQueuePushConstantRange.Build();

        RenderPipelineShaderStageBuilder compositorVertexStageBuilder;
        compositorVertexStageBuilder.shaderModule = compositorShader.vertex;
        compositorVertexStageBuilder.name = "main";
        compositorVertexStageBuilder.shaderStage = ShaderType::Vertex;

        RenderPipelineShaderStageBuilder compositorFragmentStageBuilder;
        compositorFragmentStageBuilder.shaderModule = compositorShader.fragment;
        compositorFragmentStageBuilder.name = "main";
        compositorFragmentStageBuilder.shaderStage = ShaderType::Fragment;

        GPUExtendedHandle builtVertexStage = compositorVertexStageBuilder.Build();
        GPUExtendedHandle builtFragmentStage = compositorFragmentStageBuilder.Build();

        RenderPipelineLayoutBuilder compositorPipelineLayoutBuilder;
        compositorPipelineLayoutBuilder.AddPushConstantRange(builtCompositorQueuePushConstantRange);
        compositorPipelineLayoutBuilder.AddSetLayout(builtCompositorQueueSetLayout);

        compositorPipeline.compositorVertex = builtVertexStage;
        compositorPipeline.compositorFragment = builtFragmentStage;
        compositorPipeline.compositorLayout = compositorPipelineLayoutBuilder.Build();

        RenderPipelineBuilder compositorPipelineBuilder;
        compositorPipelineBuilder.pipelineLayout = compositorPipeline.compositorLayout;
        compositorPipelineBuilder.stages.push_back(compositorPipeline.compositorVertex);
        compositorPipelineBuilder.stages.push_back(compositorPipeline.compositorFragment);

        compositorPipeline.compositorPipeline = compositorPipelineBuilder.Build();
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

    void GraphicsCore::RequestRenderBufferCleaningWithinRegion() {
        RequestTextureCollectionCleaning(renderBuffer);
    }

    void GraphicsCore::RequestTextureCollectionCleaning(PipelineFrameBuffer frb,
                                                        float multiplier) {
        glm::vec4 backgroundColor = {
                JSON_AS_TYPE(Shared::project.propertiesMap["BackgroundColor"].at(0),
                            float),
                JSON_AS_TYPE(Shared::project.propertiesMap["BackgroundColor"].at(1),
                            float),
                JSON_AS_TYPE(Shared::project.propertiesMap["BackgroundColor"].at(2),
                            float), multiplier
        };
        float blackPtr[] = {
            0.0f, 0.0f, 0.0f, multiplier
        };
        DriverCore::ClearTextureImage(frb.rbo.colorBuffer, 0, glm::value_ptr(backgroundColor));
        DriverCore::ClearTextureImage(frb.rbo.uvBuffer, 0, blackPtr);
    }

    std::vector<float> GraphicsCore::RequestRenderWithinRegion() {
        std::vector<float> renderTimes(layers.size());
        int layerIndex = 0;
        for (auto &layer : layers) {
            float first = DriverCore::GetTime();
            layer.Render();
            renderTimes[layerIndex] = (DriverCore::GetTime() - first);
            layerIndex++;
        }
        ComputeMemoryBarier(MemoryBarrierType::ImageStoreWriteBarrier);
        PerformComposition();
        return renderTimes;
    }

    GLuint GraphicsCore::GetPreviewGPUTexture() {
        switch (outputBufferType) {
        case PreviewOutputBufferType_Color:
            return renderBuffer.rbo.colorBuffer;
        case PreviewOutputBufferType_UV:
            return renderBuffer.rbo.uvBuffer;
        }
        return renderBuffer.rbo.colorBuffer;
    }

    void GraphicsCore::DrawArrays(int size) {
        Shared::renderCalls++;
        DriverCore::DrawArrays(size);
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
        return VRAM::GenerateGPUTexture(width, height);
    }

    void GraphicsCore::ResizeRenderBuffer(int width, int height) {
        if (renderBuffer.id != 0) 
            GraphicsCore::renderBuffer.Destroy();
        GraphicsCore::renderBuffer = PipelineFrameBuffer(width, height);
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

    void GraphicsCore::PerformComposition() {
        if (compositorQueue.size() == 0) return;
        DriverCore::BeginRendering(renderBuffer.fbo);
        DriverCore::BindPipeline(compositorPipeline.compositorPipeline);
        DriverCore::SetRenderingViewport(renderBuffer.width, renderBuffer.height);
        for (auto& buffer : compositorQueue) {
            CompositorPushConstants pcs;
            pcs.viewport = glm::vec2(renderBuffer.width, renderBuffer.height);
            DriverCore::PushConstants(compositorPipeline.compositorLayout, ShaderType::Fragment, 0, sizeof(CompositorPushConstants), &pcs);

            DescriptorWriteBinding colorMapBinding;
            colorMapBinding.binding = 0;
            colorMapBinding.resource = buffer.rbo.colorBuffer;
            colorMapBinding.type = DescriptorType::Sampler;

            DescriptorWriteBinding uvMapBinding;
            uvMapBinding.binding = 1;
            uvMapBinding.resource = buffer.rbo.uvBuffer;
            uvMapBinding.type = DescriptorType::Sampler;

            DriverCore::PushDescriptors({
                colorMapBinding, uvMapBinding
            }, compositorPipeline.compositorLayout, 0);

            DriverCore::DrawArrays(3); 
            Shared::compositorCalls++;
        }
        DriverCore::EndRendering();
        compositorQueue.clear();
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