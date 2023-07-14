#include "graphics_core.h"
#include "electron_image.h"

int Electron::PixelBuffer::filtering = GL_LINEAR;

Electron::PixelBuffer::PixelBuffer(int width, int height) {
    this->pixels = std::vector<Pixel>(width * height);
    this->width = width;
    this->height = height;
}

Electron::PixelBuffer::PixelBuffer(std::vector<Pixel> pixels, int width, int height) {
    this->pixels = pixels;
    this->width = width;
    this->height = height;
}

void Electron::PixelBuffer::SetPixel(int x, int y, Pixel pixel) {
    this->pixels[x + y * this->width] = pixel;
}

Electron::Pixel Electron::PixelBuffer::GetPixel(int x, int y) {
    return this->pixels[x + y * this->width];
}

GLuint Electron::PixelBuffer::BuildGPUTexture() {
    GLuint id;
    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D, id);
    std::vector<uint8_t> textureConversion(this->width * this->height * 4);
    for (int x = 0; x < this->width; x++) {
        for (int y = 0; y < this->height; y++) {
            int index = (x + y * width) * 4;
            Pixel pixel = GetPixel(x, y);
            textureConversion[index + 0] = (uint8_t) (std::clamp(pixel.r, 0.0f, 1.0f)* 255);
            textureConversion[index + 1] = (uint8_t) (std::clamp(pixel.g, 0.0f, 1.0f) * 255);
            textureConversion[index + 2] = (uint8_t) (std::clamp(pixel.b, 0.0f, 1.0f) * 255);
            textureConversion[index + 3] = (uint8_t) (std::clamp(pixel.a, 0.0f, 1.0f) * 255);
        }
    }

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, textureConversion.data());

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filtering);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filtering);

    return id;
}

void Electron::PixelBuffer::FillColor(Pixel pixel) {
    for (int x = 0; x < width; x++) {
        for (int y = 0; y < height; y++) {
            SetPixel(x, y, pixel);
        }
    }
}

Electron::RenderBuffer::RenderBuffer(int width, int height) {
    this->color = PixelBuffer(width, height);
    this->uv = PixelBuffer(width, height);
    this->depth = PixelBuffer(width, height);
}

Electron::RenderLayer::RenderLayer(std::string layerLibrary) {
    this->layerLibrary = layerLibrary;
    this->layerImplementation = dylib("layers", layerLibrary);

    this->beginFrame = 0;
    this->endFrame = 0;
    this->frameOffset = 0;

    this->layerProcedure = layerImplementation.get_function<void(RenderLayer*)>("LayerRender");
    if (!layerProcedure) throw std::runtime_error("bad layer procedure!");
}

void Electron::RenderLayer::Render(GraphicsCore* graphics) {
    this->graphicsOwner = graphics;
    if (std::clamp(graphics->renderFrame, beginFrame, endFrame) == graphics->renderFrame)
        layerProcedure(this);
}

Electron::GraphicsCore::GraphicsCore() {
    this->previousRenderBufferTexture = -1;
    this->renderBufferTexture = -1;

    this->renderFrame = 0;
    this->renderFramerate = 60; // default render framerate
    this->renderLength = 0;

    this->outputBufferType = PreviewOutputBufferType_Color;

    RenderLayer sampleRect("rect2d_layer");
    sampleRect.beginFrame = 0;
    sampleRect.endFrame = 60;

    this->layers.push_back(sampleRect);
}

void Electron::GraphicsCore::ResizeRenderBuffer(int width, int height) {
    this->renderBuffer = RenderBuffer(width, height);
}

void Electron::GraphicsCore::RequestRenderWithinRegion(RenderRequestMetadata metadata) {
    this->renderBuffer.color.FillColor(Pixel(metadata.backgroundColor[0], metadata.backgroundColor[1], metadata.backgroundColor[2], 1));
    this->renderBuffer.depth.FillColor(Pixel(1000000, 0, 0, 1));
    this->renderBuffer.uv.FillColor(Pixel(0, 0, 0, 1));
    

    for (RenderLayer& layer : layers) {
        RenderBuffer originalRenderBuffer = this->renderBuffer;
        RenderBuffer temporaryRenderBuffer(originalRenderBuffer.color.width, originalRenderBuffer.color.height);
        temporaryRenderBuffer.color.FillColor(Pixel(0, 0, 0, 0));
        temporaryRenderBuffer.depth.FillColor(Pixel(100000000, 0, 0, 1));
        temporaryRenderBuffer.uv.FillColor(Pixel(0, 0, 0, 1));
        this->renderBuffer = temporaryRenderBuffer;
        layer.Render(this);
        temporaryRenderBuffer = this->renderBuffer;

        this->renderBuffer = originalRenderBuffer;
        for (int x = 0; x < temporaryRenderBuffer.color.width; x++) {
            for (int y = 0; y < temporaryRenderBuffer.color.height; y++) {
                float renderedDepth = temporaryRenderBuffer.depth.GetPixel(x, y).r;
                float accumulatedDepth = originalRenderBuffer.depth.GetPixel(x, y).r;
                Pixel colorPixel = temporaryRenderBuffer.color.GetPixel(x, y);
                Pixel uvPixel = temporaryRenderBuffer.uv.GetPixel(x, y);
                if (colorPixel.a == 0.0) continue;
                if (renderedDepth <= accumulatedDepth) {
                    renderBuffer.depth.SetPixel(x, y, Pixel(renderedDepth, 0, 0, 1));
                    renderBuffer.color.SetPixel(x, y, colorPixel);
                    renderBuffer.uv.SetPixel(x, y, uvPixel);
                }
            }
        }

    }
}

void Electron::GraphicsCore::CleanPreviewGPUTexture() {
    if (previousRenderBufferTexture != -1) {
        glDeleteTextures(1, &previousRenderBufferTexture);
    }
}

void Electron::GraphicsCore::BuildPreviewGPUTexture() {
    previousRenderBufferTexture = renderBufferTexture;
    renderBufferTexture = GetPreviewBufferByOutputType().BuildGPUTexture();
}

Electron::PixelBuffer& Electron::GraphicsCore::GetPreviewBufferByOutputType() {
    switch (outputBufferType) {
        case PreviewOutputBufferType_Color: return renderBuffer.color;
        case PreviewOutputBufferType_Depth: return renderBuffer.depth;
        case PreviewOutputBufferType_UV: return renderBuffer.uv;
    }
    return renderBuffer.uv;
}

Electron::PixelBuffer Electron::GraphicsCore::CreateBufferFromImage(const char* filename) {
    int width, height, channels;
    stbi_uc* image = stbi_load(filename, &width, &height, &channels, 0);

    std::vector<stbi_uc> vector_image = std::vector<stbi_uc>(image, image + width * height * channels);

    std::vector<Pixel> pixelBuffer{};
    for (int i = 0; i < vector_image.size(); i += channels) {
        pixelBuffer.push_back({
            vector_image[i + 0] / 255.0f,
            vector_image[i + 1] / 255.0f,
            vector_image[i + 2] / 255.0f,
            channels == 4 ? vector_image[i + 3] / 255.0f : 1.0f
        });
    }

    stbi_image_free(image);

    return Electron::PixelBuffer(pixelBuffer, width, height);
}