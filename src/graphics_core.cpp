#include "graphics_core.h"
#include "electron_image.h"

int Electron::PixelBuffer::filtering = GL_LINEAR;

static std::unordered_map<std::string, dylib> dylibRegistry{};

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
    print("Loading dylib " + layerLibrary);

    this->beginFrame = 0;
    this->endFrame = 0;
    this->frameOffset = 0;

    dylib* implementation = nullptr;
    if (dylibRegistry.find(layerLibrary) == dylibRegistry.end()) {
        dylibRegistry[layerLibrary] = dylib("layers", layerLibrary);
    }   
    implementation = &dylibRegistry[layerLibrary];
    this->layerProcedure = implementation->get_function<void(RenderLayer*)>("LayerRender");
    this->layerPublicName = implementation->get_variable<std::string>("LayerName");
    if (!layerProcedure) throw std::runtime_error("bad layer procedure!");
}

void Electron::RenderLayer::Render(GraphicsCore* graphics) {
    this->graphicsOwner = graphics;
    if (std::clamp(graphics->renderFrame, beginFrame, endFrame) == graphics->renderFrame)
        layerProcedure(this);
}

Electron::json_t Electron::RenderLayer::InterpolateProperty(json_t keyframes) {
    int targetKeyframeIndex = -1;
    int keyframesLength = keyframes.size();
    int renderViewTime = graphicsOwner->renderFrame - beginFrame;
    GeneralizedPropertyType propertyType = keyframes.at(0);
    
    for (int i = 1; i < keyframesLength; i++) {
        int keyframeTimestamp = JSON_AS_TYPE(keyframes.at(i).at(0), int);
        if (renderViewTime <= keyframeTimestamp) {
            targetKeyframeIndex = i;
            break;
        }
    }

    // Make sure to keep last keyframe's value
    if (targetKeyframeIndex == -1) {
        return ExtractExactValue(keyframes.at(keyframesLength - 1));
    }

    // Set initial transformation position
    if (targetKeyframeIndex == 1) {
        return ExtractExactValue(keyframes.at(1));
    } 

    json_t beginKeyframeValue = ExtractExactValue(keyframes.at(targetKeyframeIndex - 1));
    json_t endKeyframeValue = ExtractExactValue(keyframes.at(targetKeyframeIndex));
    json_t interpolatedValue = {};

    float keyframeFrame = keyframes.at(targetKeyframeIndex).at(0);
    float interpolationPercentage = 0;
    if (targetKeyframeIndex == 1) {
        interpolationPercentage = renderViewTime / keyframeFrame;
    } else {
        float previousFrame = keyframes.at(targetKeyframeIndex - 1).at(0);
        interpolationPercentage = (renderViewTime - previousFrame) / (keyframeFrame - previousFrame);
    }

    switch (propertyType) {
        // Vector interpolation
        case GeneralizedPropertyType::Vec2:
        case GeneralizedPropertyType::Vec3: {
            if (beginKeyframeValue.size() != endKeyframeValue.size()) {
                throw std::runtime_error("malformed interpolation targets (vector size mismatch)");
            }
            for (int i = 0; i < beginKeyframeValue.size(); i++) {
                float beginFloat = beginKeyframeValue.at(i);
                float endFloat = endKeyframeValue.at(i);
                interpolatedValue.push_back(glm::mix(beginFloat, endFloat, interpolationPercentage));
            }
            break;
        }
    }

    return interpolatedValue;
}

Electron::json_t Electron::RenderLayer::ExtractExactValue(json_t property) {
    std::vector<json_t> acc{};
    for (int i = 1; i < property.size(); i++) {
        acc.push_back(property.at(i));
    }
    return acc;
}

Electron::GraphicsCore::GraphicsCore() {
    this->previousRenderBufferTexture = -1;
    this->renderBufferTexture = -1;

    this->renderFrame = 0;
    this->renderFramerate = 60; // default render framerate
    this->renderLength = 0;

    this->outputBufferType = PreviewOutputBufferType_Color;

    RenderLayer sampleRect("sdf2d_layer");
    sampleRect.beginFrame = 0;
    sampleRect.endFrame = 60;

    this->layers.push_back(sampleRect);
}

void Electron::GraphicsCore::ResizeRenderBuffer(int width, int height) {
    this->renderBuffer = RenderBuffer(width, height);
}

void Electron::GraphicsCore::RequestRenderWithinRegion(RenderRequestMetadata metadata) {
    layersRenderTime.clear();
    this->renderBuffer.color.FillColor(Pixel(metadata.backgroundColor[0], metadata.backgroundColor[1], metadata.backgroundColor[2], 1));
    this->renderBuffer.depth.FillColor(Pixel(MAX_DEPTH, 0, 0, 1));
    this->renderBuffer.uv.FillColor(Pixel(0, 0, 0, 1));
    

    for (RenderLayer& layer : layers) {
        float first = glfwGetTime();
        RenderBuffer originalRenderBuffer = this->renderBuffer;
        RenderBuffer temporaryRenderBuffer(originalRenderBuffer.color.width, originalRenderBuffer.color.height);
        temporaryRenderBuffer.color.FillColor(Pixel(0, 0, 0, 0));
        temporaryRenderBuffer.depth.FillColor(Pixel(MAX_DEPTH, 0, 0, 0));
        temporaryRenderBuffer.uv.FillColor(Pixel(0, 0, 0, 0));
        this->renderBuffer = temporaryRenderBuffer;
        layer.Render(this);
        temporaryRenderBuffer = this->renderBuffer;

        this->renderBuffer = originalRenderBuffer;
        for (int x = 0; x < temporaryRenderBuffer.color.width; x++) {
            for (int y = 0; y < temporaryRenderBuffer.color.height; y++) {
                Pixel colorPixel = temporaryRenderBuffer.color.GetPixel(x, y);
                Pixel uvPixel = temporaryRenderBuffer.uv.GetPixel(x, y);
                Pixel depthPixel = temporaryRenderBuffer.depth.GetPixel(x, y);
                float renderedDepth = depthPixel.r;
                float accumulatedDepth = originalRenderBuffer.depth.GetPixel(x, y).r;
                if (colorPixel.a == 0.0f) continue;
                if (renderedDepth <= accumulatedDepth || depthPixel.a == 0.0f) {
                    if (depthPixel.a != 0.0f) renderBuffer.depth.SetPixel(x, y, Pixel(renderedDepth, 0, 0, 1));
                    renderBuffer.color.SetPixel(x, y, colorPixel);
                    renderBuffer.uv.SetPixel(x, y, uvPixel);
                }
            }
        }
        float second = glfwGetTime();
        layersRenderTime.push_back(second - first);
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