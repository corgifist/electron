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
            textureConversion[index + 0] = (uint8_t) (pixel.r * 255);
            textureConversion[index + 1] = (uint8_t) (pixel.g * 255);
            textureConversion[index + 2] = (uint8_t) (pixel.b * 255);
            textureConversion[index + 3] = (uint8_t) (pixel.a * 255);
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

Electron::GraphicsCore::GraphicsCore() {
    this->previousRenderBufferTexture = -1;
    this->renderBufferTexture = -1;
}

void Electron::GraphicsCore::ResizeRenderBuffer(int width, int height) {
    this->renderBuffer = PixelBuffer(width, height);
}

void Electron::GraphicsCore::RequestRenderWithinRegion(RenderRequestMetadata metadata) {
    this->renderBuffer.FillColor(Pixel(metadata.backgroundColor[0], metadata.backgroundColor[1], metadata.backgroundColor[2], 1));

    for (int x = metadata.beginX; x < metadata.endX; x++) {
        for (int y = metadata.beginX; y < metadata.endY; y++) {
            renderBuffer.SetPixel(x, y, Pixel{(float) x / (float) metadata.endX, (float) y / (float) metadata.endY, 1, 1});
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
    renderBufferTexture = renderBuffer.BuildGPUTexture();
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