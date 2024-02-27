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
    std::vector<PipelineFrameBuffer> GraphicsCore::compositorQueue;
    GLuint GraphicsCore::pipeline;
    PipelineShader GraphicsCore::basic;
    PipelineShader GraphicsCore::compositor;
    PipelineShader GraphicsCore::channel;



    void GraphicsCore::Initialize() {

        GraphicsCore::renderFrame = 0;
        GraphicsCore::renderFramerate = 60; // default render framerate
        GraphicsCore::renderLength = 0;

        GraphicsCore::outputBufferType = PreviewOutputBufferType_Color;
    }

    void GraphicsCore::PrecompileEssentialShaders() {
        basic = CompilePipelineShader("basic.pipeline", ShaderType::Vertex);
        compositor =
            CompilePipelineShader("compositor_forward.pipeline", ShaderType::Fragment);
        channel =
            CompilePipelineShader("channel_manipulator.pipeline", ShaderType::Fragment);
        Shared::fsVAO = GenerateVAO(fsQuadVertices, fsQuadUV);

        glGenProgramPipelines(1, &pipeline);
        glBindProgramPipeline(pipeline);
        UseShader(GL_VERTEX_SHADER_BIT, basic.vertex);
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

        return -1;
    }

    void GraphicsCore::AddRenderLayer(RenderLayer layer) {
        layers.push_back(layer);
    }

    void GraphicsCore::RequestRenderBufferCleaningWithinRegion() {
        RequestTextureCollectionCleaning(renderBuffer);
    }

    void GraphicsCore::RequestTextureCollectionCleaning(PipelineFrameBuffer frb,
                                                        float multiplier) {
        glm::vec4 backgroundColor = {
                JSON_AS_TYPE(Shared::project.propertiesMap["BackgroundColor"].at(0),
                            float),
                JSON_AS_TYPE(Shared::project.propertiesMap["BackgroundColor"].at(1),
                            float),
                JSON_AS_TYPE(Shared::project.propertiesMap["BackgroundColor"].at(2),
                            float), multiplier
        };
        float blackPtr[] = {
            0.0f, 0.0f, 0.0f, 1.0f
        };
        glClearTexImage(frb.rbo.colorBuffer, 0, GL_RGBA, GL_FLOAT, glm::value_ptr(backgroundColor));
        glClearTexImage(frb.rbo.uvBuffer, 0, GL_RGBA, GL_FLOAT, blackPtr);
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
        PerformComposition();
        return renderTimes;
    }

    GLuint GraphicsCore::GetPreviewGPUTexture() {
        switch (outputBufferType) {
        case PreviewOutputBufferType_Color:
            return renderBuffer.rbo.colorBuffer;
        case PreviewOutputBufferType_UV:
            return renderBuffer.rbo.uvBuffer;
        }
        return renderBuffer.rbo.colorBuffer;
    }

    GLuint GraphicsCore::GenerateVAO(std::vector<float> vertices,
                                    std::vector<float> uv) {

        std::vector<float> unifiedData;
        for (int i = 0; i < vertices.size(); i += 2) {
            int vertexIndex = i;
            unifiedData.push_back(vertices[vertexIndex + 0]);
            unifiedData.push_back(vertices[vertexIndex + 1]);
            unifiedData.push_back(uv[vertexIndex + 0]);
            unifiedData.push_back(uv[vertexIndex + 1]);
        }
        GLuint unifiedVBO;
        GLuint vao;
        glCreateBuffers(1, &unifiedVBO);

        glCreateVertexArrays(1, &vao);

        glNamedBufferStorage(unifiedVBO, unifiedData.size() * sizeof(GLfloat),
                    unifiedData.data(), 0);

        glEnableVertexArrayAttrib(vao, 0);
        glVertexArrayAttribFormat(vao, 0, 2, GL_FLOAT, GL_FALSE, 0);
        glVertexArrayAttribBinding(vao, 0, 0);

        glEnableVertexArrayAttrib(vao, 1);
        glVertexArrayAttribFormat(vao, 1, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(GLfloat));
        glVertexArrayAttribBinding(vao, 1, 0);

        glVertexArrayVertexBuffer(vao, 0, unifiedVBO, 0, 4 * sizeof(GLfloat));

        return vao;
    }

    void GraphicsCore::DrawArrays(GLuint vao, int size) {
        Shared::renderCalls++;
        glBindVertexArray(vao);
        glDrawArrays(GL_TRIANGLES, 0, size);
    }

    GLuint GraphicsCore::CompileComputeShader(std::string path) {
        std::string computeShaderCode = read_file("compute/" + path);
        computeShaderCode =
            string_format(
                "%s\nlayout(local_size_x = "
                "%i, local_size_y = %i, local_size_z = %i) in;\n\n",
                Shared::glslVersion.c_str(), Wavefront::x, Wavefront::y, Wavefront::z) +
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

    PipelineShader GraphicsCore::CompilePipelineShader(std::string path, ShaderType type) {
        PipelineShader shader;
        path = "compute/" + path;
        std::string vertex = "";
        std::string fragment = "";
        std::vector<std::string> lines = split_string(read_file(path), "\n");
        int appendState = -1; // 0 - vertex shader, 1 - fragment shader
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

        vertex = Shared::glslVersion + "\n" + vertex;
        fragment = Shared::glslVersion + "\n" + fragment;

        const char *cVertex = vertex.c_str();
        const char *cFragment = fragment.c_str();

        if (type == ShaderType::Vertex || type == ShaderType::VertexFragment) {
            shader.vertex = glCreateShaderProgramv(GL_VERTEX_SHADER, 1, &cVertex);
            std::vector<char> log(512);
            GLsizei length;
            glGetProgramInfoLog(shader.vertex, 512, &length, log.data());
            if (length != 0) {
                DUMP_VAR(vertex);
                throw std::runtime_error(std::string(log.data()));
            }
        }

        if (type == ShaderType::Fragment || type == ShaderType::VertexFragment) {
            shader.fragment = glCreateShaderProgramv(GL_FRAGMENT_SHADER, 1, &cFragment);
            std::vector<char> log(512);
            GLsizei length;
            glGetProgramInfoLog(shader.fragment, 512, &length, log.data());
            if (length != 0) {
                DUMP_VAR(fragment);
                throw std::runtime_error(std::string(log.data()));
            }
        }

        return shader;
    }

    void GraphicsCore::UseShader(GLbitfield stage, GLuint shader) {
        glUseProgramStages(pipeline, stage, shader);
    }

    GLuint GraphicsCore::GenerateGPUTexture(int width, int height) {
        return VRAM::GenerateGPUTexture(width, height);
    }

    void GraphicsCore::ResizeRenderBuffer(int width, int height) {
        if (renderBuffer.id != 0) 
            GraphicsCore::renderBuffer.Destroy();
        GraphicsCore::renderBuffer = PipelineFrameBuffer(width, height);
    }

    void GraphicsCore::DispatchComputeShader(int grid_x, int grid_y, int grid_z) {
        glDispatchCompute(std::ceil(grid_x / Wavefront::x),
                        std::ceil(grid_y / Wavefront::y),
                        std::ceil(grid_z / Wavefront::z));
    }

    void GraphicsCore::ComputeMemoryBarier(GLbitfield barrier) {
        glMemoryBarrier(barrier);
    }

    void GraphicsCore::BindGPUTexture(GLuint texture, GLuint shader, int unit,
                                    std::string uniform) {
        glBindTextureUnit(unit, texture);
        GraphicsCore::ShaderSetUniform(shader, uniform, unit);
    }

    void GraphicsCore::BindComputeGPUTexture(GLuint texture, GLuint unit, GLuint readStatus) {
        glBindImageTexture(unit, texture, 0, GL_FALSE, 0, readStatus,
                       GL_RGBA8);
    }

    void GraphicsCore::ShaderSetUniform(GLuint program, std::string name, int x,
                                        int y) {
        glProgramUniform2i(program, GetUniformLocation(program, name), x, y);
    }

    void GraphicsCore::ShaderSetUniform(GLuint program, std::string name,
                                        glm::vec3 vec) {
        glProgramUniform3f(program, GetUniformLocation(program, name), vec.x, vec.y, vec.z);
    }

    void GraphicsCore::ShaderSetUniform(GLuint program, std::string name, float f) {
        glProgramUniform1f(program, GetUniformLocation(program, name), f);
    }

    void GraphicsCore::ShaderSetUniform(GLuint program, std::string name,
                                        glm::vec2 vec) {
        glProgramUniform2f(program, GetUniformLocation(program, name), vec.x, vec.y);
    }

    void GraphicsCore::ShaderSetUniform(GLuint program, std::string name, int x) {
        glProgramUniform1i(program, GetUniformLocation(program, name), x);
    }

    void GraphicsCore::ShaderSetUniform(GLuint program, std::string name,
                                        glm::vec4 vec) {
        glProgramUniform4f(program, GetUniformLocation(program, name), vec.x, vec.y, vec.z, vec.w);
    }

    void GraphicsCore::ShaderSetUniform(GLuint program, std::string name,
                                        glm::mat3 mat3) {
        glProgramUniformMatrix3fv(program, GetUniformLocation(program, name), 1, GL_FALSE,
                        glm::value_ptr(mat3));
    }

    void GraphicsCore::ShaderSetUniform(GLuint program, std::string name,
                                        glm::mat4 mat4) {
        glProgramUniformMatrix4fv(program, GetUniformLocation(program, name), 1, GL_FALSE,
                        glm::value_ptr(mat4));
    }

    void GraphicsCore::ShaderSetUniform(GLuint program, std::string name, bool b) {
        ShaderSetUniform(program, name, (int) b);
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

    void GraphicsCore::CallCompositor(PipelineFrameBuffer frb) {
        compositorQueue.push_back(frb);
    }

    void GraphicsCore::PerformComposition() {
        if (compositorQueue.size() == 0) return;
        renderBuffer.Bind();
        glBindVertexArray(Shared::fsVAO);
        
        static GLuint samplersBuffer = 0;
        if (samplersBuffer == 0) {
            glCreateBuffers(1, &samplersBuffer);
            glNamedBufferStorage(samplersBuffer, SAMPLER_BUFFER_SIZE * 2 * sizeof(GLuint64), NULL, GL_DYNAMIC_STORAGE_BIT | PERSISTENT_FLAG);
        }

        int residentsCount = glm::min((size_t) SAMPLER_BUFFER_SIZE, compositorQueue.size());
        static std::vector<GLuint64> handles;
        static std::vector<int> ids;
        bool bufferNeedsUpdate = false;
        if (!ids.empty()) {
            for (int i = 0; i < ids.size(); i++) {
                if (ids[i] != compositorQueue[i].id) {
                    bufferNeedsUpdate = true;
                    break;
                }
            }
        }
        if (ids.empty() || bufferNeedsUpdate) {
            handles.clear();
            ids.clear();
            for (int i = 0; i < residentsCount; i++) {
                PipelineFrameBuffer& frb = compositorQueue[i];
                ids.push_back(frb.id);
                handles.push_back(frb.colorHandle);
                handles.push_back(frb.uvHandle);
            }
            glNamedBufferSubData(samplersBuffer, 0, handles.size() * sizeof(GLuint64), handles.data());
        }

        static glm::mat4 identity = glm::mat4(1);
        UseShader(GL_FRAGMENT_SHADER_BIT, compositor.fragment);
        ShaderSetUniform(basic.vertex, "uMatrix", identity);

        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, samplersBuffer);
        glDrawArraysInstancedBaseInstance(GL_TRIANGLES, 0, fsQuadVertices.size() / 2, residentsCount, 0);

        compositorQueue.erase(compositorQueue.begin(), compositorQueue.begin() + residentsCount);
        Shared::compositorCalls++;
        if (compositorQueue.size() != 0) {
            PerformComposition();
        }
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