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

        glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
        std::vector<int> maybeSize = {
            JSON_AS_TYPE(Shared::configMap["LastWindowSize"].at(0), int),
            JSON_AS_TYPE(Shared::configMap["LastWindowSize"].at(1), int)};
        GLFWwindow* displayHandle = glfwCreateWindow(
            maybeSize[0],
            maybeSize[1], "Electron", nullptr, nullptr);

        glfwMakeContextCurrent(displayHandle);
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
        glEnable(GL_DEBUG_OUTPUT);
        glDebugMessageCallback(GLCallback, 0);
        glClearColor(0, 0, 0, 1);

        renderer.displayHandle = (void*) displayHandle;
        renderer.vendor = (const char*) glGetString(GL_VENDOR);
        renderer.version = (const char*) glGetString(GL_VERSION);
        renderer.renderer = (const char*) glGetString(GL_RENDERER);

        DUMP_VAR(renderer.vendor);
        DUMP_VAR(renderer.version);
        DUMP_VAR(renderer.renderer);
    }

    bool DriverCore::ShouldClose() {
        return glfwWindowShouldClose((GLFWwindow*) renderer.displayHandle);
    }

    void DriverCore::SwapBuffers() {
        glfwSwapBuffers((GLFWwindow*) renderer.displayHandle);
    }

    void DriverCore::ImGuiNewFrame() {
        glfwWaitEvents();
        glClear(GL_COLOR_BUFFER_BIT);

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
    }

    void DriverCore::ImGuiRender() {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        ImGui::Render();
        ImGui::EndFrame();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    }

    void DriverCore::ImGuiShutdown() {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
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