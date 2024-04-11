#pragma once
#include "electron.h"
#include "pixel_buffer.h"
#include "driver_core.h"

namespace Electron {

    struct VRAM {
        static GPUExtendedHandle GenerateGPUTexture(GPUExtendedHandle contextPtr, int width, int height);
    };

    // A set of GPU textures that represents color/depth/uv buffers
    class RenderBuffer {
    public:
        GPUExtendedHandle colorBuffer;
        GPUExtendedHandle uvBuffer;
        int width, height;

        RenderBuffer(GPUExtendedHandle context, int width, int height);
        RenderBuffer() {
            this->width = 0;
            this->height = 0;
        };

        void Destroy();

        ~RenderBuffer();
    };

    // GPUTexture that adapts to preview buffer size changes
    struct ResizableGPUTexture {
        GPUExtendedHandle texture;
        int width, height;

        ResizableGPUTexture(GPUExtendedHandle context, int width, int height);
        ResizableGPUTexture() {}

        void CheckForResize(GPUExtendedHandle context, RenderBuffer* rbo);
        void Destroy();
    };

    // RenderBuffer that adapts to preview buffer size changes
    struct ResizableRenderBuffer {
        ResizableGPUTexture color, uv;

        ResizableRenderBuffer(GPUExtendedHandle context, int width, int height);
        ResizableRenderBuffer() {}

        void CheckForResize(GPUExtendedHandle context, RenderBuffer* rbo);
        void Destroy();
    };

    struct PipelineFrameBuffer {
        GPUExtendedHandle fbo;
        RenderBuffer rbo;
        int width, height;
        int id;
        static int counter;

        PipelineFrameBuffer(GPUExtendedHandle context, int width, int height);
        PipelineFrameBuffer(GPUExtendedHandle color, GPUExtendedHandle uv);
        PipelineFrameBuffer() {
            this->id = 0;
        }

        void Destroy();
    };

}