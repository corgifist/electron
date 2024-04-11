#include "vram.h"

namespace Electron {

    GPUExtendedHandle VRAM::GenerateGPUTexture(GPUExtendedHandle contextPtr, int width, int height) {
        return DriverCore::GenerateGPUTexture(contextPtr, width, height);
    }

    int PipelineFrameBuffer::counter = 1;

    RenderBuffer::RenderBuffer(GPUExtendedHandle context, int width, int height) {
        this->colorBuffer = VRAM::GenerateGPUTexture(context, width, height);
        this->uvBuffer = VRAM::GenerateGPUTexture(context, width, height);

        this->width = width;
        this->height = height;
    }

    RenderBuffer::~RenderBuffer() {}

    void RenderBuffer::Destroy() {
        PixelBuffer::DestroyGPUTexture(colorBuffer);
        PixelBuffer::DestroyGPUTexture(uvBuffer);
    }

    ResizableGPUTexture::ResizableGPUTexture(GPUExtendedHandle context, int width, int height) {
        this->width = width;
        this->height = height;
        this->texture = VRAM::GenerateGPUTexture(context, width, height);
    }

    void ResizableGPUTexture::CheckForResize(GPUExtendedHandle context, RenderBuffer *pbo) {
        if (width != pbo->width || height != pbo->height) {
            this->width = pbo->width;
            this->height = pbo->height;
            PixelBuffer::DestroyGPUTexture(texture);
            this->texture = VRAM::GenerateGPUTexture(context, width, height);
        }
    }

    void ResizableGPUTexture::Destroy() { PixelBuffer::DestroyGPUTexture(texture); }

    ResizableRenderBuffer::ResizableRenderBuffer(GPUExtendedHandle context, int width, int height) {
        this->color = ResizableGPUTexture(context, width, height);
        this->uv = ResizableGPUTexture(context, width, height);
    }

    void ResizableRenderBuffer::CheckForResize(GPUExtendedHandle context, RenderBuffer *pbo) {
        color.CheckForResize(context, pbo);
        uv.CheckForResize(context, pbo);
    }

    void ResizableRenderBuffer::Destroy() {
        color.Destroy();
        uv.Destroy();
    }

    PipelineFrameBuffer::PipelineFrameBuffer(GPUExtendedHandle context, int width, int height) {
        this->width = width;
        this->height = height;
        this->id = counter++;
        this->rbo = RenderBuffer(context, width, height);
        this->fbo = DriverCore::GenerateFramebuffer(rbo.colorBuffer, rbo.uvBuffer, width, height);
    }

    PipelineFrameBuffer::PipelineFrameBuffer(GPUExtendedHandle color, GPUExtendedHandle uv) {
        this->rbo.colorBuffer = color;
        this->rbo.uvBuffer = uv;
        this->id = counter++;
    }

    void PipelineFrameBuffer::Destroy() {
        rbo.Destroy();
        DriverCore::DestroyFramebuffer(fbo);
    }
}