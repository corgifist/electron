#include "pixel_buffer.h"

namespace Electron {
    int PixelBuffer::filtering = GL_LINEAR;

    PixelBuffer::PixelBuffer(int width, int height) {
        this->pixels = std::vector<Pixel>(width * height);
        this->width = width;
        this->height = height;
}

PixelBuffer::PixelBuffer(std::vector<Pixel> pixels, int width,
                                   int height) {
    this->pixels = pixels;
    this->width = width;
    this->height = height;
}

PixelBuffer::PixelBuffer(std::string path) {
    int width, height, channels;
    if (!file_exists(path)) {
        throw std::runtime_error("image " + path + " is not found");
    }
    stbi_uc *image = stbi_load(path.c_str(), &width, &height, &channels, 0);

    std::vector<stbi_uc> vector_image =
        std::vector<stbi_uc>(image, image + width * height * channels);

    std::vector<Pixel> pixelBuffer{};
    for (int i = 0; i < vector_image.size(); i += channels) {
        pixelBuffer.push_back(
            {vector_image[i + 0] / 255.0f, vector_image[i + 1] / 255.0f,
             vector_image[i + 2] / 255.0f,
             channels == 4 ? vector_image[i + 3] / 255.0f : 1.0f});
    }

    stbi_image_free(image);

    *this = PixelBuffer(pixelBuffer, width, height);
}

void PixelBuffer::SetPixel(int x, int y, Pixel pixel) {
    if (x >= this->width || y >= this->height)
        return; // discard out of bounds pixels
    if (0 > this->width || 0 > this->height)
        return;
    this->pixels[x + y * this->width] = pixel;
}

Pixel PixelBuffer::GetPixel(int x, int y) {
    return this->pixels[x + y * this->width];
}

GLuint PixelBuffer::BuildGPUTexture() {
    GLuint id;
    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D, id);
    std::vector<float> textureConversion(this->width * this->height * 4);
    for (int x = 0; x < this->width; x++) {
        for (int y = 0; y < this->height; y++) {
            int index = (x + y * width) * 4;
            Pixel pixel = GetPixel(x, y);
            textureConversion[index + 0] = pixel.r;
            textureConversion[index + 1] = pixel.g;
            textureConversion[index + 2] = pixel.b;
            textureConversion[index + 3] = pixel.a;
        }
    }

    // PixelBuffer is a pretty low-level API, so we are using raw GLES functions here
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA32F, width, height);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filtering);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filtering);

    glUseProgram(Shared::tex_transfer_compute);
    glUniform2i(glGetUniformLocation(Shared::tex_transfer_compute, "imageResolution"), width, height);

    GLuint data_ssbo;
    glGenBuffers(1, &data_ssbo);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, data_ssbo);
    glBufferData(GL_SHADER_STORAGE_BUFFER, textureConversion.size() * sizeof(float), textureConversion.data(), GL_STATIC_READ);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, data_ssbo);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, id);
    glBindImageTexture(0, id, 0, GL_FALSE, 0, GL_WRITE_ONLY,
                       GL_RGBA32F);

    glDispatchCompute(std::ceil(width / Wavefront::x), std::ceil(height / Wavefront::y), 1);
    glMemoryBarrier(GL_ALL_BARRIER_BITS);
    glDeleteBuffers(1, &data_ssbo);

    return id;
}

void PixelBuffer::FillColor(Pixel pixel) {
    for (int x = 0; x < width; x++) {
        for (int y = 0; y < height; y++) {
            SetPixel(x, y, pixel);
        }
    }
}

void PixelBuffer::DestroyGPUTexture(GLuint texture) {
    glDeleteTextures(1, &texture);
}
}