#include "async_rendering.h"

namespace Electron {
    AsyncRenderingData AsyncRendering::data = {};
    PipelineFrameBuffer AsyncRendering::renderBuffer;
    bool AsyncRendering::presentSuccessfull = false;

    static struct CompositorPipeline {
        GPUExtendedHandle compositorVertex, compositorFragment;
        GPUExtendedHandle compositorLayout;
        GPUExtendedHandle compositorPipeline;
    } compositorPipeline;

    static std::mutex resizeCommandsMutex;

    void AsyncRendering::Initialize() {
        data.renderContext = DriverCore::GenerateContext();

        auto compositorShader = GraphicsCore::CompilePipelineShader("compositor");

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

        presentSuccessfull = false;

        data.terminateRendererTask = false;
        data.allowRendering = false;
        data.requestedRenderFrame = 0.0f;
        data.rendererTask = new std::thread([]() {
            while (!data.terminateRendererTask) {
                AsyncRendering::RenderFrame();
            }
        });
    }

    void AsyncRendering::RenderFrame() {
        if (!data.allowRendering) return;
        while (!presentSuccessfull) {}
        presentSuccessfull = false;
        int firstMillisecondsTime = DriverCore::GetTime() * 1000;
        GPUExtendedHandle currentContext = data.renderContext;
        RenderLayerRenderDescription renderDescription = {};
        renderDescription.context = currentContext;
        renderDescription.frame = data.requestedRenderFrame;

        AsyncRendering::SwapRenderBuffers();
        DriverCore::WaitContextFence(currentContext);

        resizeCommandsMutex.lock();
        for (auto& resize : data.resizeCommands) {
            for (auto& renderBufferImage : data.renderBuffers) {
                DriverCore::DestroyImageHandleUI(renderBufferImage.colorHandle);
                if (renderBufferImage.frameBuffer.id != 0) renderBufferImage.frameBuffer.Destroy();
            }
            data.renderBuffers.clear();
            for (int i = 0; i < 2; i++) {
                AsyncRenderingRenderBuffer renderBufferImage;
                renderBufferImage.frameBuffer = PipelineFrameBuffer(currentContext, resize.width, resize.height);
                renderBufferImage.colorHandle = DriverCore::GetImageHandleUI(renderBufferImage.frameBuffer.rbo.colorBuffer);
                data.renderBuffers.push_back(renderBufferImage);
            }
            AsyncRendering::renderBuffer = data.renderBuffers[0].frameBuffer;
        } 
        data.resizeCommands.clear();
        resizeCommandsMutex.unlock();

        DriverCore::BeginContext(currentContext);
        if (data.renderBuffers.size() <= 0) return;
        GraphicsCore::RequestTextureCollectionCleaning(currentContext, data.renderBuffers[1].frameBuffer);
        int layerIndex = 0;
        for (auto &layer : GraphicsCore::layers) {
            if (!layer.initialized) continue;
            layer.Render(renderDescription);
        }
        AsyncRendering::PerformComposition(currentContext, data.renderBuffers[1].frameBuffer.fbo);
        DriverCore::EndContext(currentContext);

        int secondTimeMilliseconds = DriverCore::GetTime() * 1000;
        int sleepTarget = glm::max(16 - (secondTimeMilliseconds - firstMillisecondsTime), 0);
        if (sleepTarget > 0)
            std::this_thread::sleep_for(std::chrono::milliseconds(sleepTarget));
    }

    void AsyncRendering::PerformComposition(GPUExtendedHandle context, GPUExtendedHandle targetRenderBuffer) {
        if (GraphicsCore::compositorQueue.size() == 0) return;
        DriverCore::BeginRendering(context, targetRenderBuffer);
        DriverCore::BindPipeline(context, compositorPipeline.compositorPipeline);
        DriverCore::SetRenderingViewport(context, renderBuffer.width, renderBuffer.height);
        for (auto& buffer : GraphicsCore::compositorQueue) {
            CompositorPushConstants pcs;
            pcs.viewport = glm::vec2(renderBuffer.width, renderBuffer.height);
            DriverCore::PushConstants(context, compositorPipeline.compositorLayout, ShaderType::Fragment, 0, sizeof(CompositorPushConstants), &pcs);

            DescriptorWriteBinding colorMapBinding;
            colorMapBinding.binding = 0;
            colorMapBinding.resource = buffer.rbo.colorBuffer;
            colorMapBinding.type = DescriptorType::Sampler;

            DescriptorWriteBinding uvMapBinding;
            uvMapBinding.binding = 1;
            uvMapBinding.resource = buffer.rbo.uvBuffer;
            uvMapBinding.type = DescriptorType::Sampler;

            DriverCore::PushDescriptors(context, {
                colorMapBinding, uvMapBinding
            }, compositorPipeline.compositorLayout, 0);

            DriverCore::DrawArrays(context, 3); 
            Shared::compositorCalls++;
        }
        DriverCore::EndRendering(context);
        GraphicsCore::compositorQueue.clear();
    }

    GPUExtendedHandle AsyncRendering::GetRenderBufferImageHandle() {
        if (data.renderBuffers.size() <= 0) return 0;
        return data.renderBuffers[0].colorHandle;
    }

    void AsyncRendering::AllowRendering(bool allow) {
        data.allowRendering = allow;
    }

    void AsyncRendering::RequestFrame(float frame) {
        data.requestedRenderFrame = frame;
    }

    void AsyncRendering::SwapRenderBuffers() {
        if (data.renderBuffers.size() <= 0) return;
        std::swap(data.renderBuffers[1], data.renderBuffers[0]);
        renderBuffer = data.renderBuffers[0].frameBuffer;
    }

    void AsyncRendering::ResizeRenderBuffer(int width, int height) {
        resizeCommandsMutex.lock();
        data.resizeCommands.push_back(
            ResizeRenderBufferDescription{
                .width = width,
                .height = height
            }
        );
        resizeCommandsMutex.unlock();
    }

    

    void AsyncRendering::Terminate() {
        data.terminateRendererTask = true;
        data.rendererTask->join();
        renderBuffer.Destroy();
    }

};