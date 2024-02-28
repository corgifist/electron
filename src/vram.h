#pragma once
#include "electron.h"
#include "pixel_buffer.h"

#define PERSISTENT_FLAG GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT

namespace Electron {

    struct VRAM {
        static GLuint GenerateGPUTexture(int width, int height);
    };

    // A set of GPU textures that represents color/depth/uv buffers
    class RenderBuffer {
    public:
        GLuint colorBuffer;
        GLuint uvBuffer;
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
        ResizableGPUTexture color, uv;

        ResizableRenderBuffer(int width, int height);
        ResizableRenderBuffer() {}

        void CheckForResize(RenderBuffer* rbo);
        void Destroy();
    };

    struct PipelineFrameBuffer {
        GLuint fbo, stencil;
        RenderBuffer rbo;
        int width, height;
        int id;
        static int counter;

        PipelineFrameBuffer(int width, int height);
        PipelineFrameBuffer(GLuint color, GLuint uv);
        PipelineFrameBuffer() {
            this->id = 0;
        }

        void Bind();
        void Unbind();
        void Destroy();
    };

}