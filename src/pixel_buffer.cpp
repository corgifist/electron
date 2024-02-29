#include "pixel_buffer.h"

namespace Electron {
    int PixelBuffer::filtering = GL_LINEAR;

    PixelBuffer::PixelBuffer(int width, int height) {
        this->pixels = std::vector<Pixel>(width * height);
        this->width = width;
        this->height = height;
    }

    PixelBuffer::PixelBuffer(std::vector<Pixel> pixels, int width, int height) {
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
        std::vector<GLubyte> textureConversion(this->width * this->height * 4);
        for (int x = 0; x < this->width; x++) {
            for (int y = 0; y < this->height; y++) {
                int index = (x + y * width) * 4;
                Pixel pixel = GetPixel(x, y);
                textureConversion[index + 0] = (GLubyte)(pixel.r * 255.0f);
                textureConversion[index + 1] = (GLubyte)(pixel.g * 255.0f);
                textureConversion[index + 2] = (GLubyte)(pixel.b * 255.0f);
                textureConversion[index + 3] = (GLubyte)(pixel.a * 255.0f);
            }
        }

        id = DriverCore::ImportGPUTexture(textureConversion.data(), width, height, 4);

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
        DriverCore::DestroyGPUTexture(texture);
    }
} // namespace Electron