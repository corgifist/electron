#pragma once
#include "electron.h"
#include "driver_core.h"
#include "graphics_core.h"

namespace Electron {

    struct ResizeRenderBufferDescription {
        int width, height;
    };

    struct AsyncRenderingRenderBuffer {
        PipelineFrameBuffer frameBuffer;
        GPUExtendedHandle colorHandle;
    };

    struct AsyncRenderingData {
        float requestedRenderFrame;
        GPUExtendedHandle renderContext;
        std::vector<ResizeRenderBufferDescription> resizeCommands;
        std::vector<AsyncRenderingRenderBuffer> renderBuffers;
        std::thread* rendererTask;
        bool terminateRendererTask;
        bool allowRendering;
    };

    struct AsyncRendering {
        static AsyncRenderingData data;
        static PipelineFrameBuffer renderBuffer; // front buffer

        static void Initialize();
        static void RenderFrame();
        static void AllowRendering(bool allow);
        static void RequestFrame(float frame);
        static void ResizeRenderBuffer(int width, int height);
        static void PerformComposition(GPUExtendedHandle context, GPUExtendedHandle targetRenderBuffer);
        static GPUExtendedHandle GetRenderBufferImageHandle();
        static void SwapRenderBuffers();
        static void Terminate();
    };  
};