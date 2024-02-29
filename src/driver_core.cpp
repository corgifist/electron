#include "driver_core.h"

namespace Electron {

    RendererInfo DriverCore::renderer{};

    void DriverCore::GLFWCallback(int id, const char *description) {
        print("DriverCore::GLFWCallback: " << id);
        print("DriverCore::GLFWCallback: " << description);
    }

    static void GLAPIENTRY GLCallback( GLenum source,
                 GLenum type,
                 GLuint id,
                 GLenum severity,
                 GLsizei length,
                 const GLchar* message,
                 const void* userParam ) {
        if (type != GL_DEBUG_TYPE_ERROR && type != GL_DEBUG_TYPE_PERFORMANCE) return;
        fprintf( stderr, "%s\n",
            message );
    }   


    void DriverCore::Bootstrap() {
        print("bootstraping graphics api rn!!!");
        std::string sessionType = getEnvVar("XDG_SESSION_TYPE");
        if (JSON_AS_TYPE(Shared::configMap["PreferX11"], bool) == true) {
            sessionType = "x11";
        }
        int platformType =
            sessionType == "x11" ? GLFW_PLATFORM_X11 : GLFW_PLATFORM_WAYLAND;
        glfwSetErrorCallback(DriverCore::GLFWCallback);
        glfwInitHint(GLFW_PLATFORM, platformType);
        print("using " << sessionType << " platform");
        if (!glfwPlatformSupported(platformType)) {
            print("requested platform (" << sessionType << ") is unavailable!");
            glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_X11);
        }
        if (!glfwInit()) {
            throw std::runtime_error("cannot initialize glfw!");
        }
        int major, minor, rev;
        glfwGetVersion(&major, &minor, &rev);
        print("GLFW version: " << major << "." << minor << " " << rev);

        glfwDefaultWindowHints();
        glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
        glfwWindowHint(GLFW_VISIBLE, GLFW_TRUE);

        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
        std::vector<int> maybeSize = {
            JSON_AS_TYPE(Shared::configMap["LastWindowSize"].at(0), int),
            JSON_AS_TYPE(Shared::configMap["LastWindowSize"].at(1), int)};
        GLFWwindow* displayHandle = glfwCreateWindow(
            maybeSize[0],
            maybeSize[1], "Electron", nullptr, nullptr);

        glfwMakeContextCurrent(displayHandle);
        glfwSwapInterval(0);
        if (!gladLoadGL((GLADloadfunc) glfwGetProcAddress)) {
            throw std::runtime_error(
                "cannot load opengl es function pointers!");
        }
        glfwSetFramebufferSizeCallback(
            displayHandle,
            [](GLFWwindow *display, int width, int height) {
                glViewport(0, 0, width, height);
            });

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO &io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.ConfigFlags |= ImGuiConfigFlags_DpiEnableScaleFonts;
        io.ConfigWindowsMoveFromTitleBarOnly = true;
        io.WantSaveIniSettings = true;
        io.IniFilename = "misc/imgui.ini";

        ImGui_ImplGlfw_InitForOpenGL(displayHandle, true);
        ImGui_ImplOpenGL3_Init();
        DUMP_VAR(ImGui::GetIO().BackendRendererName);

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glEnable(GL_DITHER);
        // glEnable(GL_DEBUG_OUTPUT);
        // glDebugMessageCallback(GLCallback, 0);

        renderer.displayHandle = (void*) displayHandle;
        renderer.vendor = (const char*) glGetString(GL_VENDOR);
        renderer.version = (const char*) glGetString(GL_VERSION);
        renderer.renderer = (const char*) glGetString(GL_RENDERER);

        DUMP_VAR(renderer.vendor);
        DUMP_VAR(renderer.version);
        DUMP_VAR(renderer.renderer);
        print("OpenGL Extensions:");
        int extensionsCount;
        glGetIntegerv(GL_NUM_EXTENSIONS, &extensionsCount);
        for (int i = 0; i < extensionsCount; i++) {
            std::string extension = (const char*) glGetStringi(GL_EXTENSIONS, i);
            print("\t" << glGetStringi(GL_EXTENSIONS, i));
        }
    }

    GPUHandle DriverCore::GeneratePipeline() {
        GPUHandle pipeline;
        glCreateProgramPipelines(1, &pipeline);
        glBindProgramPipeline(pipeline);
        return pipeline;
    }

    void DriverCore::BindPipeline(GPUHandle pipeline) {
        glBindProgramPipeline(pipeline);
    }

    GPUHandle DriverCore::GenerateGPUTexture(int width, int height) {
        GLuint texture;
        if (width < 1 || height < 1)
            throw std::runtime_error("malformed texture dimensions");
        glCreateTextures(GL_TEXTURE_2D, 1, &texture);
        glTextureStorage2D(texture, 1, GL_RGBA8, width, height);
        glTextureParameteri(texture, GL_TEXTURE_MIN_FILTER,
                        GL_LINEAR);
        glTextureParameteri(texture, GL_TEXTURE_MAG_FILTER,
                        GL_LINEAR);
        glGenerateTextureMipmap(texture);

        return texture;
    }

    GPUHandle DriverCore::ImportGPUTexture(uint8_t* texture, int width, int height, int channels) {
        GPUHandle id;
        glCreateTextures(GL_TEXTURE_2D, 1, &id);
        glTextureStorage2D(id, 1, GL_RGBA8, width, height);
        glTextureSubImage2D(id, 0, 0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, texture);
        glTextureParameteri(id, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTextureParameteri(id, GL_TEXTURE_MAG_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glGenerateTextureMipmap(id);
        return id;
    }

    GPUExtendedHandle DriverCore::GenerateFramebuffer(GPUHandle color, GPUHandle uv, int width, int height) {
        FramebufferInfo* info = new FramebufferInfo();
        info->width = width;
        info->height = height;
        glCreateFramebuffers(1, &info->fbo);

        glNamedFramebufferTexture(info->fbo, GL_COLOR_ATTACHMENT0, color, 0);
        glNamedFramebufferTexture(info->fbo, GL_COLOR_ATTACHMENT1, uv, 0);

        glCreateRenderbuffers(1, &info->stencil);
        glNamedRenderbufferStorage(info->stencil, GL_DEPTH24_STENCIL8, width, height);
        glNamedFramebufferRenderbuffer(info->fbo, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, info->stencil);

        GLuint attachments[2] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1};
        glNamedFramebufferDrawBuffers(info->fbo, 2, attachments);
        if (glCheckNamedFramebufferStatus(info->fbo, GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            throw std::runtime_error("framebuffer is not complete!");
        }
        return (GPUExtendedHandle) info;
    }

    void DriverCore::UseProgramStages(ShaderType shaderType, GPUHandle pipeline, GPUHandle shader) {
        GLbitfield shaderBitfield = 0;
        switch (shaderType) {
            case ShaderType::Vertex: {
                shaderBitfield = GL_VERTEX_SHADER_BIT;
                break;
            }
            case ShaderType::Fragment: {
                shaderBitfield = GL_FRAGMENT_SHADER_BIT;
                break;
            }
            case ShaderType::VertexFragment: {
                shaderBitfield = GL_VERTEX_SHADER_BIT | GL_FRAGMENT_SHADER_BIT;
                break;
            }
            case ShaderType::Compute: {
                shaderBitfield = GL_COMPUTE_SHADER_BIT;
                break;
            }
        }
        glUseProgramStages(pipeline, shaderBitfield, shader);
    }

    void DriverCore::DispatchCompute(int x, int y, int z) {
        glDispatchCompute(x, y, z);
    }

    void DriverCore::ClearTextureImage(GPUHandle texture, int attachment, float* color) {
        glClearTexImage(texture, 0, GL_RGBA, GL_FLOAT, color);
    }

    GPUExtendedHandle DriverCore::GenerateVAO(std::vector<float>& vertices, std::vector<float>& uv) {
        VAOInfo* vao = new VAOInfo();
        
        std::vector<float> unifiedData;
        for (int i = 0; i < vertices.size(); i += 2) {
            int vertexIndex = i;
            unifiedData.push_back(vertices[vertexIndex + 0]);
            unifiedData.push_back(vertices[vertexIndex + 1]);
            unifiedData.push_back(uv[vertexIndex + 0]);
            unifiedData.push_back(uv[vertexIndex + 1]);
        }
        glCreateBuffers(1, &vao->buffer);

        glCreateVertexArrays(1, &vao->vao);

        glNamedBufferStorage(vao->buffer, unifiedData.size() * sizeof(GLfloat),
                    unifiedData.data(), 0);

        glEnableVertexArrayAttrib(vao->vao, 0);
        glVertexArrayAttribFormat(vao->vao, 0, 2, GL_FLOAT, GL_FALSE, 0);
        glVertexArrayAttribBinding(vao->vao, 0, 0);

        glEnableVertexArrayAttrib(vao->vao, 1);
        glVertexArrayAttribFormat(vao->vao, 1, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(GLfloat));
        glVertexArrayAttribBinding(vao->vao, 1, 0);

        glVertexArrayVertexBuffer(vao->vao, 0, vao->buffer, 0, 4 * sizeof(GLfloat));

        return (GPUExtendedHandle) vao;
    }

    void DriverCore::DestroyVAO(GPUExtendedHandle vao) {
        VAOInfo* info = (VAOInfo*) vao;
        glDeleteBuffers(1, &info->buffer);
        glDeleteVertexArrays(1, &info->vao);
        delete info;
    }

    void DriverCore::BindVAO(GPUExtendedHandle vao) {
        VAOInfo* info = (VAOInfo*) vao;
        static GPUHandle boundVAO = 0;
        if (boundVAO != info->vao) {
            glBindVertexArray(info->vao);
            boundVAO = info->vao;
        }
    }

    void DriverCore::DrawArrays(int size) {
        glDrawArrays(GL_TRIANGLES, 0, size);
    }

    GPUHandle DriverCore::GenerateShaderProgram(ShaderType shader, const char* code) {
        GLenum enumType = 0;
        std::string strType = "";
        switch (shader) {
            case ShaderType::Vertex: {
                enumType = GL_VERTEX_SHADER;
                strType = "vertex";
                break;
            }
            case ShaderType::Fragment: {
                enumType = GL_FRAGMENT_SHADER;
                strType = "fragment";
                break;
            }
            case ShaderType::Compute: {
                enumType = GL_COMPUTE_SHADER;
                strType = "compute";
                break;
            }
        }

        GPUHandle program = glCreateShaderProgramv(enumType, 1, &code);
        std::vector<char> log(512);
        GLsizei length;
        glGetProgramInfoLog(program, 512, &length, log.data());
        if (length != 0) {
            DUMP_VAR(code);
            throw std::runtime_error(std::string(log.data()));
        }
        return program;
    }

    GPUHandle DriverCore::GetProgramUniformLocation(GPUHandle program, const char* uniform) {
        return glGetUniformLocation(program, uniform);
    }

    void DriverCore::DestroyShaderProgram(GPUHandle program) {
        glDeleteProgram(program);
    }

    void DriverCore::MemoryBarrier(MemoryBarrierType type) {
        GLbitfield bitfield = 0;
        switch (type) {
            case MemoryBarrierType::ImageStoreWriteBarrier: {
                bitfield = GL_SHADER_IMAGE_ACCESS_BARRIER_BIT;
                break;
            }
        }
        glMemoryBarrier(bitfield);
    }

    void DriverCore::DestroyFramebuffer(GPUExtendedHandle fbo) {
        FramebufferInfo* info = (FramebufferInfo*) fbo;
        glDeleteRenderbuffers(1, &info->stencil);
        glDeleteFramebuffers(1, &info->fbo);
        delete info;
    }

    void DriverCore::BindFramebuffer(GPUExtendedHandle fbo) {
        if (!fbo) {
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glViewport(0, 0, Shared::displaySize.x, Shared::displaySize.y);
        } else {
            FramebufferInfo* info = (FramebufferInfo*) fbo;
            glBindFramebuffer(GL_FRAMEBUFFER, info->fbo);
            glViewport(0, 0, info->width, info->height);
        }
    }

    void DriverCore::DestroyGPUTexture(GPUHandle texture) {
        glDeleteTextures(1, &texture);
    }

    bool DriverCore::ShouldClose() {
        return glfwWindowShouldClose((GLFWwindow*) renderer.displayHandle);
    }

    void DriverCore::SwapBuffers() {
        Shared::frameID++;
        glfwSwapBuffers((GLFWwindow*) renderer.displayHandle);
    }

    void DriverCore::ImGuiNewFrame() {
        glfwWaitEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
    }

    void DriverCore::ImGuiRender() {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        float blackPtr[] = {
            0, 0, 0, 1
        };
        glClearBufferfv(GL_COLOR, 0, blackPtr);
        ImGui::Render();
        ImGui::EndFrame();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    }

    void DriverCore::ImGuiShutdown() {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        glfwDestroyWindow((GLFWwindow*) renderer.displayHandle);
        glfwTerminate();
    }

    void DriverCore::SetTitle(std::string title) {
        glfwSetWindowTitle((GLFWwindow*) renderer.displayHandle, title.c_str());
    }

    void DriverCore::GetDisplayPos(int* x, int* y) {
        glfwGetWindowPos((GLFWwindow*) renderer.displayHandle, x, y);
    }

    void DriverCore::GetDisplaySize(int* w, int* h) {
        glfwGetWindowSize((GLFWwindow*) renderer.displayHandle, w, h);
    }

    double DriverCore::GetTime() {
        return glfwGetTime();
    }
}