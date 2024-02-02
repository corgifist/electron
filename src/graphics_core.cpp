#include "graphics_core.h"

namespace Electron {

    std::unordered_map<GLuint, std::unordered_map<std::string, GLuint>>
        GraphicsCore::uniformCache{};

    PipelineFrameBuffer GraphicsCore::renderBuffer;
    PreviewOutputBufferType GraphicsCore::outputBufferType;
    std::vector<RenderLayer> GraphicsCore::layers;
    bool GraphicsCore::isPlaying;
    float GraphicsCore::renderFrame;
    int GraphicsCore::renderLength, GraphicsCore::renderFramerate;

    RenderBuffer::RenderBuffer(int width, int height) {
        this->colorBuffer = GraphicsCore::GenerateGPUTexture(width, height, 0);
        this->uvBuffer = GraphicsCore::GenerateGPUTexture(width, height, 1);
        this->depthBuffer = GraphicsCore::GenerateGPUTexture(width, height, 2);

        this->width = width;
        this->height = height;
    }

    RenderBuffer::~RenderBuffer() {}

    void RenderBuffer::Destroy() {
        PixelBuffer::DestroyGPUTexture(colorBuffer);
        PixelBuffer::DestroyGPUTexture(uvBuffer);
        PixelBuffer::DestroyGPUTexture(depthBuffer);
    }

    void GraphicsCore::Initialize() {

        GraphicsCore::renderFrame = 0;
        GraphicsCore::renderFramerate = 60; // default render framerate
        GraphicsCore::renderLength = 0;

        GraphicsCore::outputBufferType = PreviewOutputBufferType_Color;
    }

    void GraphicsCore::PrecompileEssentialShaders() {
        Shared::basic_compute = CompilePipelineShader("basic.pipeline");
        Shared::compositor_compute =
            CompilePipelineShader("compositor_forward.pipeline");
        Shared::channel_manipulator_compute =
            CompilePipelineShader("channel_manipulator.pipeline");
        Shared::fsVAO = GenerateVAO(fsQuadVertices, fsQuadUV);
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
        GraphicsCore::renderBuffer.Destroy();
        GraphicsCore::renderBuffer = PipelineFrameBuffer(width, height);
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

    void ResizableGPUTexture::Destroy() { PixelBuffer::DestroyGPUTexture(texture); }

    ResizableRenderBuffer::ResizableRenderBuffer(int width, int height) {
        this->color = ResizableGPUTexture(width, height);
        this->uv = ResizableGPUTexture(width, height);
        this->depth = ResizableGPUTexture(width, height);
    }

    void ResizableRenderBuffer::CheckForResize(RenderBuffer *pbo) {
        color.CheckForResize(pbo);
        uv.CheckForResize(pbo);
        depth.CheckForResize(pbo);
    }

    void ResizableRenderBuffer::Destroy() {
        color.Destroy();
        uv.Destroy();
        depth.Destroy();
    }

    PipelineFrameBuffer::PipelineFrameBuffer(int width, int height) {
        this->width = width;
        this->height = height;
        this->rbo = RenderBuffer(width, height);
        glGenFramebuffers(1, &fbo);
        Bind();

        glBindTexture(GL_TEXTURE_2D, rbo.colorBuffer);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                            rbo.colorBuffer, 0);

        glBindTexture(GL_TEXTURE_2D, rbo.uvBuffer);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D,
                            rbo.uvBuffer, 0);

        glBindTexture(GL_TEXTURE_2D, rbo.depthBuffer);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D,
                            rbo.depthBuffer, 0);

        glGenRenderbuffers(1, &stencil);
        glBindRenderbuffer(GL_RENDERBUFFER, stencil);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
                                GL_RENDERBUFFER, stencil);

        GLuint attachments[3] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1,
                                GL_COLOR_ATTACHMENT2};
        glDrawBuffers(3, attachments);
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            throw std::runtime_error("framebuffer is not complete!");
        }
        Unbind();
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
        rbo.Destroy();
        glDeleteRenderbuffers(1, &stencil);
        glDeleteFramebuffers(1, &fbo);
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
        RequestTextureCollectionCleaning(renderBuffer);
    }

    void GraphicsCore::RequestTextureCollectionCleaning(PipelineFrameBuffer frb,
                                                        float multiplier) {

        UseShader(Shared::basic_compute);

        frb.Bind();
        ShaderSetUniform(
            Shared::basic_compute, "backgroundColor",
            glm::vec3(
                JSON_AS_TYPE(Shared::project.propertiesMap["BackgroundColor"].at(0),
                            float),
                JSON_AS_TYPE(Shared::project.propertiesMap["BackgroundColor"].at(1),
                            float),
                JSON_AS_TYPE(Shared::project.propertiesMap["BackgroundColor"].at(2),
                            float)));
        ShaderSetUniform(Shared::basic_compute, "multiplier", multiplier);
        glBindVertexArray(Shared::fsVAO);
        glDrawArrays(GL_TRIANGLES, 0, fsQuadVertices.size() / 2);
        frb.Unbind();
    }

    std::vector<float> GraphicsCore::RequestRenderWithinRegion() {
        std::vector<float> renderTimes(layers.size());
        int layerIndex = 0;
        for (auto &layer : layers) {
            float first = DriverCore::GetTime();
            layer.Render();
            renderTimes[layerIndex] = (DriverCore::GetTime() - first);
            layerIndex++;
        }
        return renderTimes;
    }

    GLuint GraphicsCore::GetPreviewGPUTexture() {
        switch (outputBufferType) {
        case PreviewOutputBufferType_Color:
            return renderBuffer.rbo.colorBuffer;
        case PreviewOutputBufferType_Depth:
            return renderBuffer.rbo.depthBuffer;
        case PreviewOutputBufferType_UV:
            return renderBuffer.rbo.uvBuffer;
        }
        return renderBuffer.rbo.colorBuffer;
    }

    GLuint GraphicsCore::GenerateVAO(std::vector<float> vertices,
                                    std::vector<float> uv) {
        GLuint verticesVBO, uvVBO;
        GLuint vao;
        glGenBuffers(1, &verticesVBO);
        glGenBuffers(1, &uvVBO);

        glGenVertexArrays(1, &vao);
        glBindVertexArray(vao);

        glBindBuffer(GL_ARRAY_BUFFER, verticesVBO);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(GLfloat),
                    vertices.data(), GL_STATIC_DRAW);

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(GLfloat),
                            (void *)0);

        glBindBuffer(GL_ARRAY_BUFFER, uvVBO);
        glBufferData(GL_ARRAY_BUFFER, uv.size() * sizeof(GLfloat), uv.data(),
                    GL_STATIC_DRAW);

        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float),
                            (void *)0);

        return vao;
    }

    GLuint GraphicsCore::CompileComputeShader(std::string path) {
        std::string computeShaderCode = read_file("compute/" + path);
        computeShaderCode =
            string_format(
                "#version 310 es\nprecision highp float;\nlayout(local_size_x = "
                "%i, local_size_y = %i, local_size_z = %i) in;\n\n",
                Wavefront::x, Wavefront::y, Wavefront::z) +
            computeShaderCode;
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
            print((const char *)errorLog.data());

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

            throw std::runtime_error("error while linking compute shader!\n" +
                                    std::string((char *)infoLog.data()));

            return -1;
        }

        glDeleteShader(computeShader);

        return computeProgram;
    }

    GLuint GraphicsCore::CompilePipelineShader(std::string path) {
        path = "compute/" + path;
        std::string vertex = "";
        std::string fragment = "";
        std::vector<std::string> lines = split_string(read_file(path), "\n");
        int appendState = 0; // 0 - vertex shader, 1 - fragment shader
        for (auto &line : lines) {
            if (line.find("#vertex") != std::string::npos)
                appendState = 0;
            else if (line.find("#fragment") != std::string::npos)
                appendState = 1;
            if (appendState == 0 && line.find("#vertex") == std::string::npos)
                vertex += line + "\n";
            else if (appendState == 1 &&
                    line.find("#fragment") == std::string::npos)
                fragment += line + "\n";
        }

        vertex = Shared::glslVersion + "\nprecision highp float;\n" + vertex;
        fragment = Shared::glslVersion + "\nprecision highp float;\n" + fragment;

        const char *cVertex = vertex.c_str();
        const char *cFragment = fragment.c_str();

        GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
        GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
        GLuint program = glCreateProgram();

        glShaderSource(vertexShader, 1, &cVertex, NULL);
        glCompileShader(vertexShader);

        int success;
        char infoLog[512];
        glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
        if (!success) {
            glGetShaderInfoLog(vertexShader, 512, NULL, infoLog);
            throw std::runtime_error(infoLog);
        }

        glShaderSource(fragmentShader, 1, &cFragment, NULL);
        glCompileShader(fragmentShader);
        glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
        if (!success) {
            glGetShaderInfoLog(fragmentShader, 512, NULL, infoLog);
            throw std::runtime_error(infoLog);
        }

        glAttachShader(program, vertexShader);
        glAttachShader(program, fragmentShader);
        glLinkProgram(program);
        glGetProgramiv(program, GL_LINK_STATUS, &success);
        if (!success) {
            glGetProgramInfoLog(program, 512, NULL, infoLog);
            throw std::runtime_error(infoLog);
        }

        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);
        return program;
    }

    void GraphicsCore::UseShader(GLuint shader) { glUseProgram(shader); }

    void GraphicsCore::DispatchComputeShader(int grid_x, int grid_y, int grid_z) {
        glDispatchCompute(std::ceil(grid_x / Wavefront::x),
                        std::ceil(grid_y / Wavefront::y),
                        std::ceil(grid_z / Wavefront::z));
    }

    void GraphicsCore::ComputeMemoryBarier(GLbitfield barrier) {
        glMemoryBarrier(barrier);
    }

    GLuint GraphicsCore::GenerateGPUTexture(int width, int height, int unit) {
        unsigned int texture;
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, width, height, 0, GL_RGBA,
                    GL_FLOAT, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                        PixelBuffer::filtering);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
                        PixelBuffer::filtering);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,
                        GL_REPEAT); // set texture wrapping to GL_REPEAT (default
                                    // wrapping method)
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

        return texture;
    }

    void GraphicsCore::BindGPUTexture(GLuint texture, GLuint shader, int unit,
                                    std::string uniform) {
        glActiveTexture(GL_TEXTURE0 + unit);
        glBindTexture(GL_TEXTURE_2D, texture);
        glUniform1i(glGetUniformLocation(shader, uniform.c_str()), unit);
    }

    void GraphicsCore::ShaderSetUniform(GLuint program, std::string name, int x,
                                        int y) {
        glUniform2i(GetUniformLocation(program, name), x, y);
    }

    void GraphicsCore::ShaderSetUniform(GLuint program, std::string name,
                                        glm::vec3 vec) {
        glUniform3f(GetUniformLocation(program, name), vec.x, vec.y, vec.z);
    }

    void GraphicsCore::ShaderSetUniform(GLuint program, std::string name, float f) {
        glUniform1f(GetUniformLocation(program, name), f);
    }

    void GraphicsCore::ShaderSetUniform(GLuint program, std::string name,
                                        glm::vec2 vec) {
        glUniform2f(GetUniformLocation(program, name), vec.x, vec.y);
    }

    void GraphicsCore::ShaderSetUniform(GLuint program, std::string name, int x) {
        glUniform1i(GetUniformLocation(program, name), x);
    }

    void GraphicsCore::ShaderSetUniform(GLuint program, std::string name,
                                        glm::vec4 vec) {
        glUniform4f(GetUniformLocation(program, name), vec.x, vec.y, vec.z, vec.w);
        ;
    }

    void GraphicsCore::ShaderSetUniform(GLuint program, std::string name,
                                        glm::mat3 mat3) {
        glUniformMatrix3fv(GetUniformLocation(program, name), 1, GL_FALSE,
                        glm::value_ptr(mat3));
    }

    void GraphicsCore::ShaderSetUniform(GLuint program, std::string name,
                                        glm::mat4 mat4) {
        glUniformMatrix4fv(GetUniformLocation(program, name), 1, GL_FALSE,
                        glm::value_ptr(mat4));
    }

    GLuint GraphicsCore::GetUniformLocation(GLuint program, std::string name) {
        std::unordered_map<std::string, GLuint> programMap{};
        if (uniformCache.find(program) != uniformCache.end())
            programMap = uniformCache[program];
        GLuint location = 0;
        if (programMap.find(name) != programMap.end()) {
            return programMap[name];
        } else {
            location = glGetUniformLocation(program, name.c_str());
            programMap[name] = location;
        }
        uniformCache[program] = programMap;
        return location;
    }

    // Please use DefferCompositor Instead
    void GraphicsCore::CallCompositor(PipelineFrameBuffer frb) {

        renderBuffer.Bind();
        UseShader(Shared::compositor_compute);
        BindGPUTexture(frb.rbo.colorBuffer, Shared::compositor_compute, 0,
                    "lColor");
        BindGPUTexture(frb.rbo.uvBuffer, Shared::compositor_compute, 1, "lUV");
        BindGPUTexture(frb.rbo.depthBuffer, Shared::compositor_compute, 2,
                    "lDepth");

        glBindVertexArray(Shared::fsVAO);
        glDrawArrays(GL_TRIANGLES, 0, fsQuadVertices.size() / 2);
        renderBuffer.Unbind();
    }

    void GraphicsCore::FireTimelineSeek() {
        for (auto &layer : layers) {
            layer.onTimelineSeek(&layer);
        }
    }

    void GraphicsCore::FirePlaybackChange() {
        for (auto &layer : layers) {
            layer.onPlaybackChangeProcedure(&layer);
        }
    }

} // namespace Electron