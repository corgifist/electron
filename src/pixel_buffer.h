#pragma once

#include "electron.h"
#include "utils/stb_image.h"
#include "shared.h"

namespace Electron {

    struct Pixel {
        float r, g, b, a;

        Pixel(float r, float g, float b, float a) {
            this->r = r;
            this->g = g;
            this->b = b;
            this->a = a;
        };

        Pixel() = default;
    };

    // Simple CPUTexture buffer
    struct PixelBuffer {
        int width, height;
        static int filtering;
        std::vector<Pixel> pixels;

        PixelBuffer(int width, int height);
        PixelBuffer(std::vector<Pixel> pixels, int width, int height);
        PixelBuffer(std::string path);
        PixelBuffer() {};

        void SetPixel(int x, int y, Pixel);
        Pixel GetPixel(int x, int y);

        void FillColor(Pixel pixel);

        GLuint BuildGPUTexture();

        static void DestroyGPUTexture(GLuint texture);
    };
}