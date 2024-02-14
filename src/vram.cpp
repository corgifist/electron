#include "vram.h"

namespace Electron {

    GLuint VRAM::GenerateGPUTexture(int width, int height) {
        GLuint texture;
        if (width < 1 || height < 1)
            throw std::runtime_error("malformed texture dimensions");
        glCreateTextures(GL_TEXTURE_2D, 1, &texture);
        glTextureStorage2D(texture, 1, GL_RGBA8, width, height);
        glTextureParameteri(texture, GL_TEXTURE_MIN_FILTER,
                        GL_LINEAR);
        glTextureParameteri(texture, GL_TEXTURE_MAG_FILTER,
                        GL_LINEAR);
        glGenerateTextureMipmap(texture);

        return texture;
    }

    int PipelineFrameBuffer::counter = 0;

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
        this->resident = false;
        glCreateFramebuffers(1, &fbo);

        glNamedFramebufferTexture(fbo, GL_COLOR_ATTACHMENT0, rbo.colorBuffer, 0);
        glNamedFramebufferTexture(fbo, GL_COLOR_ATTACHMENT1, rbo.uvBuffer, 0);

        glCreateRenderbuffers(1, &stencil);
        glNamedRenderbufferStorage(stencil, GL_DEPTH24_STENCIL8, width, height);
        glNamedFramebufferRenderbuffer(fbo, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, stencil);

        GLuint attachments[2] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1};
        glNamedFramebufferDrawBuffers(fbo, 2, attachments);
        if (glCheckNamedFramebufferStatus(fbo, GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            throw std::runtime_error("framebuffer is not complete!");
        }

        this->colorHandle = glGetTextureHandleARB(rbo.colorBuffer);
        if (!colorHandle) {
            throw std::runtime_error("cannot get texture handle for color buffer");
        }
        this->uvHandle = glGetTextureHandleARB(rbo.uvBuffer);
        if (!uvHandle) {
            throw std::runtime_error("cannot get texture handle for uv buffer!");
        }
        MakeResident();
    }

    void PipelineFrameBuffer::Bind() {
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glViewport(0, 0, width, height);
    }

    void PipelineFrameBuffer::Unbind() {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, Shared::displaySize.x, Shared::displaySize.y);
    }

    void PipelineFrameBuffer::Destroy() {
        if (resident) MakeNonResident();
        rbo.Destroy();
        glDeleteRenderbuffers(1, &stencil);
        glDeleteFramebuffers(1, &fbo);
    }

    void PipelineFrameBuffer::MakeResident() {
        if (resident) return;
        glMakeTextureHandleResidentARB(colorHandle);
        glMakeTextureHandleResidentARB(uvHandle);
        resident = true;
    }

    void PipelineFrameBuffer::MakeNonResident() {
        if (!resident) return;
        glMakeTextureHandleNonResidentARB(colorHandle);
        glMakeTextureHandleNonResidentARB(uvHandle);
        resident = false;
    }
}