#include "vram.h"

namespace Electron {

    GPUHandle VRAM::GenerateGPUTexture(int width, int height) {
        return DriverCore::GenerateGPUTexture(width, height);
    }

    int PipelineFrameBuffer::counter = 1;

    RenderBuffer::RenderBuffer(int width, int height) {
        this->colorBuffer = VRAM::GenerateGPUTexture(width, height);
        this->uvBuffer = VRAM::GenerateGPUTexture(width, height);

        this->width = width;
        this->height = height;
    }

    RenderBuffer::~RenderBuffer() {}

    void RenderBuffer::Destroy() {
        PixelBuffer::DestroyGPUTexture(colorBuffer);
        PixelBuffer::DestroyGPUTexture(uvBuffer);
    }

    ResizableGPUTexture::ResizableGPUTexture(int width, int height) {
        this->width = width;
        this->height = height;
        this->texture = VRAM::GenerateGPUTexture(width, height);
    }

    void ResizableGPUTexture::CheckForResize(RenderBuffer *pbo) {
        if (width != pbo->width || height != pbo->height) {
            this->width = pbo->width;
            this->height = pbo->height;
            PixelBuffer::DestroyGPUTexture(texture);
            this->texture = VRAM::GenerateGPUTexture(width, height);
        }
    }

    void ResizableGPUTexture::Destroy() { PixelBuffer::DestroyGPUTexture(texture); }

    ResizableRenderBuffer::ResizableRenderBuffer(int width, int height) {
        this->color = ResizableGPUTexture(width, height);
        this->uv = ResizableGPUTexture(width, height);
    }

    void ResizableRenderBuffer::CheckForResize(RenderBuffer *pbo) {
        color.CheckForResize(pbo);
        uv.CheckForResize(pbo);
    }

    void ResizableRenderBuffer::Destroy() {
        color.Destroy();
        uv.Destroy();
    }

    PipelineFrameBuffer::PipelineFrameBuffer(int width, int height) {
        this->width = width;
        this->height = height;
        this->id = counter++;
        this->rbo = RenderBuffer(width, height);
        this->fbo = DriverCore::GenerateFramebuffer(rbo.colorBuffer, rbo.uvBuffer, width, height);
    }

    PipelineFrameBuffer::PipelineFrameBuffer(GLuint color, GLuint uv) {
        this->rbo.colorBuffer = color;
        this->rbo.uvBuffer = uv;
        this->id = counter++;
    }

    void PipelineFrameBuffer::Bind() {
        DriverCore::BindFramebuffer(fbo);
    }

    void PipelineFrameBuffer::Unbind() {
        DriverCore::BindFramebuffer(0);
    }

    void PipelineFrameBuffer::Destroy() {
        rbo.Destroy();
        DriverCore::DestroyFramebuffer(fbo);
    }
}