#pragma once
#include "electron.h"
#include "pixel_buffer.h"
#include "driver_core.h"

#define PERSISTENT_FLAG GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT

namespace Electron {

    struct VRAM {
        static GPUExtendedHandle GenerateGPUTexture(int width, int height);
    };

    // A set of GPU textures that represents color/depth/uv buffers
    class RenderBuffer {
    public:
        GPUExtendedHandle colorBuffer;
        GPUExtendedHandle uvBuffer;
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
        GPUExtendedHandle texture;
        int width, height;

        ResizableGPUTexture(int width, int height);
        ResizableGPUTexture() {}

        void CheckForResize(RenderBuffer* rbo);
        void Destroy();
    };

    // RenderBuffer that adapts to preview buffer size changes
    struct ResizableRenderBuffer {
        ResizableGPUTexture color, uv;

        ResizableRenderBuffer(int width, int height);
        ResizableRenderBuffer() {}

        void CheckForResize(RenderBuffer* rbo);
        void Destroy();
    };

    struct PipelineFrameBuffer {
        GPUExtendedHandle fbo;
        RenderBuffer rbo;
        int width, height;
        int id;
        static int counter;

        PipelineFrameBuffer(int width, int height);
        PipelineFrameBuffer(GPUExtendedHandle color, GPUExtendedHandle uv);
        PipelineFrameBuffer() {
            this->id = 0;
        }

        void Destroy();
    };

}