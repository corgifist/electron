#include "graphics_core.h"

namespace Electron {
RenderBuffer::RenderBuffer(GraphicsCore *core, int width,
                                     int height) {
    this->colorBuffer = core->GenerateGPUTexture(width, height, 0);
    this->uvBuffer = core->GenerateGPUTexture(width, height, 1);
    this->depthBuffer = core->GenerateGPUTexture(width, height, 2);

    this->width = width;
    this->height = height;
}

RenderBuffer::~RenderBuffer() {}


GraphicsCore::GraphicsCore() {

    this->renderFrame = 0;
    this->renderFramerate = 60; // default render framerate
    this->renderLength = 0;

    this->outputBufferType = PreviewOutputBufferType_Color;
}

void GraphicsCore::PrecompileEssentialShaders() {
    Shared::basic_compute = CompileComputeShader("basic.compute");
    Shared::compositor_compute = CompileComputeShader("compositor.compute");
    Shared::tex_transfer_compute = CompileComputeShader("tex_transfer.compute");
}

void GraphicsCore::FetchAllLayers() {
    auto iterator = std::filesystem::directory_iterator("layers");
    for (auto &entry : iterator) {
        std::string transformedPath = std::regex_replace(
            base_name(entry.path().string()), std::regex(".dll|.so|lib"), "");
        Libraries::LoadLibrary("layers", transformedPath);
        print("Pre-render fetching " << transformedPath);
        dylibRegistry.push_back(transformedPath);
    }
    print("Fetched all layers");
}

DylibRegistry GraphicsCore::GetImplementationsRegistry() {
    return dylibRegistry;
}

void GraphicsCore::ResizeRenderBuffer(int width, int height) {
    this->renderBuffer = RenderBuffer(this, width, height);
}

ResizableGPUTexture::ResizableGPUTexture(int width, int height) {
    this->width = width;
    this->height = height;
    this->texture = GraphicsCore::GenerateGPUTexture(width, height, 0);
}

void ResizableGPUTexture::CheckForResize(RenderBuffer *pbo) {
    if (width != pbo->width || height != pbo->height) {
        this->width = pbo->width;
        this->height = pbo->height;
        PixelBuffer::DestroyGPUTexture(texture);
        this->texture = GraphicsCore::GenerateGPUTexture(width, height, 0);
    }
}

void ResizableGPUTexture::Destroy() {
    PixelBuffer::DestroyGPUTexture(texture);
}

ResizableRenderBuffer::ResizableRenderBuffer(int width, int height) {
    this->color = ResizableGPUTexture(width, height);
    this->uv = ResizableGPUTexture(width, height);
    this->depth = ResizableGPUTexture(width, height);
}

void ResizableRenderBuffer::CheckForResize(RenderBuffer* pbo) {
    color.CheckForResize(pbo);
    uv.CheckForResize(pbo);
    depth.CheckForResize(pbo);
}

void ResizableRenderBuffer::Destroy() {
    color.Destroy();
    uv.Destroy();
    depth.Destroy();
}

RenderLayer *GraphicsCore::GetLayerByID(int id) {
    for (auto &layer : layers) {
        if (layer.id == id)
            return &layer;
    }

    throw std::runtime_error(string_format("cannot find layer with id %i", id));
}

int GraphicsCore::GetLayerIndexByID(int id) {
    int index = 0;
    for (auto &layer : layers) {
        if (layer.id == id)
            return index;
        index++;
    }

    throw std::runtime_error(string_format("cannot find layer with id %i", id));
    return index;
}

void GraphicsCore::AddRenderLayer(RenderLayer layer) {
    layers.push_back(layer);
}

void GraphicsCore::RequestRenderBufferCleaningWithinRegion() {
    /* this->renderBuffer.color.FillColor(Pixel(metadata.backgroundColor[0],
    metadata.backgroundColor[1], metadata.backgroundColor[2], 1), metadata);
    this->renderBuffer.depth.FillColor(Pixel(MAX_DEPTH, 0, 0, 1), metadata);
    this->renderBuffer.uv.FillColor(Pixel(0, 0, 0, 1), metadata);
    */

    RequestTextureCollectionCleaning(
        renderBuffer.colorBuffer, renderBuffer.uvBuffer,
        renderBuffer.depthBuffer, renderBuffer.width, renderBuffer.height);
}

void GraphicsCore::RequestTextureCollectionCleaning(
    GLuint color, GLuint uv, GLuint depth, int width, int height) {

    UseShader(Shared::basic_compute);
    BindGPUTexture(color, 0, GL_WRITE_ONLY);
    BindGPUTexture(uv, 1, GL_WRITE_ONLY);
    BindGPUTexture(depth, 2, GL_WRITE_ONLY);


    ShaderSetUniform(Shared::basic_compute, "backgroundColor",
                     glm::vec3(JSON_AS_TYPE(Shared::project.propertiesMap["BackgroundColor"].at(0), float),
                               JSON_AS_TYPE(Shared::project.propertiesMap["BackgroundColor"].at(1), float),
                               JSON_AS_TYPE(Shared::project.propertiesMap["BackgroundColor"].at(2), float)));
    ShaderSetUniform(Shared::basic_compute, "maxDepth", (float)MAX_DEPTH);
    DispatchComputeShader(renderBuffer.width,
                          renderBuffer.height, 1);
    ComputeMemoryBarier(GL_ALL_BARRIER_BITS);
}

std::vector<float> GraphicsCore::RequestRenderWithinRegion() {
    std::vector<float> renderTimes(layers.size());
    int layerIndex = 0;
    for (auto &layer : layers) {
        float first = (float) SDL_GetTicks() / 1000.0;
        layer.Render();
        renderTimes[layerIndex] = (((float) SDL_GetTicks() / 1000.0) - first);
        layerIndex++;
    }
    return renderTimes;
}

GLuint GraphicsCore::GetPreviewGPUTexture() {
    switch (outputBufferType) {
    case PreviewOutputBufferType_Color:
        return renderBuffer.colorBuffer;
    case PreviewOutputBufferType_Depth:
        return renderBuffer.depthBuffer;
    case PreviewOutputBufferType_UV:
        return renderBuffer.uvBuffer;
    }
    return renderBuffer.uvBuffer;
}

GLuint GraphicsCore::CompileComputeShader(std::string path) {
    std::string computeShaderCode = read_file("compute/" + path);
    computeShaderCode = string_format(
        "#version 310 es\nprecision highp float;\nlayout(local_size_x = %i, local_size_y = %i, local_size_z = %i) in;\n\n",
        Wavefront::x, Wavefront::y, Wavefront::z
    ) + computeShaderCode;
    const char *c_code = computeShaderCode.c_str();

    GLuint computeShader = glCreateShader(GL_COMPUTE_SHADER);
    glShaderSource(computeShader, 1, &c_code, NULL);
    glCompileShader(computeShader);

    GLint compiled = 0;
    glGetShaderiv(computeShader, GL_COMPILE_STATUS, &compiled);
    if (compiled == GL_FALSE) {
        GLint maxLength = 0;
        glGetShaderiv(computeShader, GL_INFO_LOG_LENGTH, &maxLength);

        std::vector<GLchar> errorLog(maxLength);
        glGetShaderInfoLog(computeShader, maxLength, &maxLength, &errorLog[0]);
        print((const char*) errorLog.data());

        glDeleteShader(computeShader);

        throw std::runtime_error("error while compiling compute shader!");
        return -1;
    }

    GLuint computeProgram = glCreateProgram();
    glAttachShader(computeProgram, computeShader);
    glLinkProgram(computeProgram);

    GLint isLinked = 0;
    glGetProgramiv(computeProgram, GL_LINK_STATUS, &isLinked);
    if (isLinked == GL_FALSE) {
        GLint maxLength = 0;
        glGetProgramiv(computeProgram, GL_INFO_LOG_LENGTH, &maxLength);

        std::vector<GLchar> infoLog(maxLength);
        glGetProgramInfoLog(computeProgram, maxLength, &maxLength, &infoLog[0]);

        glDeleteProgram(computeProgram);

        throw std::runtime_error("error while linking compute shader!\n" + std::string((char*) infoLog.data()));

        return -1;
    }

    glDeleteShader(computeShader);

    return computeProgram;
}

void GraphicsCore::UseShader(GLuint shader) { glUseProgram(shader); }

void GraphicsCore::DispatchComputeShader(int grid_x, int grid_y,
                                                   int grid_z) {
    glDispatchCompute(std::ceil(grid_x / Wavefront::x), std::ceil(grid_y / Wavefront::y), std::ceil(grid_z / Wavefront::z));
}

void GraphicsCore::ComputeMemoryBarier(GLbitfield barrier) {
    glMemoryBarrier(barrier);
}

GLuint GraphicsCore::GenerateGPUTexture(int width, int height,
                                                  int unit) {
    unsigned int texture;

    glGenTextures(1, &texture);
    glActiveTexture(GL_TEXTURE0 + unit);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, PixelBuffer::filtering);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, PixelBuffer::filtering);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA32F, width, height);

    return texture;
}

void GraphicsCore::BindGPUTexture(GLuint texture, int unit, int readStatus) {
    glActiveTexture(GL_TEXTURE0 + unit);
    glBindTexture(GL_TEXTURE_2D, texture);
    glBindImageTexture(unit, texture, 0, GL_FALSE, 0, readStatus,
                       GL_RGBA32F);
}

void GraphicsCore::ShaderSetUniform(GLuint program, std::string name,
                                              int x, int y) {
    glUniform2i(glGetUniformLocation(program, name.c_str()), x, y);
}

void GraphicsCore::ShaderSetUniform(GLuint program, std::string name,
                                              glm::vec3 vec) {
    glUniform3f(glGetUniformLocation(program, name.c_str()), vec.x, vec.y,
                vec.z);
}

void GraphicsCore::ShaderSetUniform(GLuint program, std::string name,
                                              float f) {
    glUniform1f(glGetUniformLocation(program, name.c_str()), f);
}

void GraphicsCore::ShaderSetUniform(GLuint program, std::string name,
                                              glm::vec2 vec) {
    glUniform2f(glGetUniformLocation(program, name.c_str()), vec.x, vec.y);
}

void GraphicsCore::ShaderSetUniform(GLuint program, std::string name,
                                              int x) {
    glUniform1i(glGetUniformLocation(program, name.c_str()), x);
}

void GraphicsCore::ShaderSetUniform(GLuint program, std::string name, glm::vec4 vec) {
    glUniform4f(glGetUniformLocation(program, name.c_str()), vec.x, vec.y, vec.z, vec.w);;
}


void GraphicsCore::CallCompositor(ResizableGPUTexture color,
                                            ResizableGPUTexture uv,
                                            ResizableGPUTexture depth) {

    UseShader(Shared::compositor_compute);
    BindGPUTexture(renderBuffer.colorBuffer, 0, GL_WRITE_ONLY);
    BindGPUTexture(renderBuffer.uvBuffer, 1, GL_WRITE_ONLY);
    BindGPUTexture(renderBuffer.depthBuffer, 2, GL_WRITE_ONLY);
    BindGPUTexture(renderBuffer.depthBuffer, 6, GL_READ_ONLY);

    BindGPUTexture(color.texture, 3, GL_READ_ONLY);
    BindGPUTexture(uv.texture, 4, GL_READ_ONLY);
    BindGPUTexture(depth.texture, 5, GL_READ_ONLY);

    DispatchComputeShader(renderBuffer.width,
                          renderBuffer.height, 1);
    ComputeMemoryBarier(GL_ALL_BARRIER_BITS);
}

void GraphicsCore::FireTimelineSeek() {
    for (auto& layer : layers) {
        layer.onTimelineSeek(&layer);
    }
}

void GraphicsCore::FirePlaybackChange() {
    for (auto& layer : layers) {
        layer.onPlaybackChangeProcedure(&layer);
    }
}

}