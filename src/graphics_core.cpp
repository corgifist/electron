#include "graphics_core.h"

namespace Electron {

    std::unordered_map<GPUHandle, std::unordered_map<std::string, GPUHandle>>
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
            CompilePipelineShader("compositor.pipeline", ShaderType::Fragment);
        channel =
            CompilePipelineShader("channel_manipulator.pipeline", ShaderType::Fragment);
        Shared::fsVAO = GenerateVAO(fsQuadVertices, fsQuadUV);

        pipeline = DriverCore::GeneratePipeline();
        UseShader(ShaderType::Vertex, basic.vertex);
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
        DriverCore::ClearTextureImage(frb.rbo.colorBuffer, 0, glm::value_ptr(backgroundColor));
        DriverCore::ClearTextureImage(frb.rbo.uvBuffer, 0, blackPtr);
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

    GPUExtendedHandle GraphicsCore::GenerateVAO(std::vector<float> vertices,
                                    std::vector<float> uv) {
        return DriverCore::GenerateVAO(vertices, uv);
    }

    void GraphicsCore::DrawArrays(GLuint vao, int size) {
        Shared::renderCalls++;
        DriverCore::BindVAO(Shared::fsVAO);
        DriverCore::DrawArrays(size);
    }

    GPUHandle GraphicsCore::CompileComputeShader(std::string path) {
        return DriverCore::GenerateShaderProgram(ShaderType::Compute, path.c_str());
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
            shader.vertex = DriverCore::GenerateShaderProgram(ShaderType::Vertex, cVertex);
        }

        if (type == ShaderType::Fragment || type == ShaderType::VertexFragment) {
            shader.fragment = DriverCore::GenerateShaderProgram(ShaderType::Fragment, cFragment);
        }

        return shader;
    }

    void GraphicsCore::UseShader(ShaderType stage, GPUHandle shader) {
        DriverCore::UseProgramStages(stage, pipeline, shader);
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
        DriverCore::DispatchCompute(std::ceil(grid_x / Wavefront::x),
                        std::ceil(grid_y / Wavefront::y),
                        std::ceil(grid_z / Wavefront::z));
    }

    void GraphicsCore::ComputeMemoryBarier(MemoryBarrierType barrier) {
        DriverCore::MemoryBarrier(barrier);
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

    GLuint GraphicsCore::GetUniformLocation(GPUHandle program, std::string name) {
        std::unordered_map<std::string, GPUHandle> programMap{};
        if (uniformCache.find(program) != uniformCache.end())
            programMap = uniformCache[program];
        GLuint location = 0;
        if (programMap.find(name) != programMap.end()) {
            return programMap[name];
        } else {
            location = DriverCore::GetProgramUniformLocation(program, name.c_str());
            programMap[name] = location;
        }
        uniformCache[program] = programMap;
        return location;
    }

    void GraphicsCore::CallCompositor(PipelineFrameBuffer frb) {
        compositorQueue.push_back(frb);
    }

    void GraphicsCore::PerformForwardComposition() {
        DriverCore::BindVAO(Shared::fsVAO);

        glm::mat4 identity = glm::identity<glm::mat4>();
        renderBuffer.Bind();
        UseShader(ShaderType::Vertex, basic.vertex);
        UseShader(ShaderType::Fragment, compositor.fragment);
        ShaderSetUniform(basic.vertex, "uMatrix", identity);
        ShaderSetUniform(compositor.fragment, "uDisplaySize", glm::vec2(renderBuffer.width, renderBuffer.height));

        for (auto& buffer : compositorQueue) {
            BindGPUTexture(buffer.rbo.colorBuffer, compositor.fragment, 0, "uColor");
            BindGPUTexture(buffer.rbo.uvBuffer, compositor.fragment, 1, "uUV");

            DriverCore::DrawArrays(fsQuadVertices.size() / 2);
            Shared::compositorCalls++;
        }
        renderBuffer.Unbind();
        compositorQueue.clear();
    }

    void GraphicsCore::PerformComposition() {
        PerformForwardComposition();
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