#include "app.h"

static void electronGlfwError(int id, const char* description) {
    print("GLFW_ERROR_ID: " << id);
    print("GLFW_ERROR: " << description);
}

Electron::AppInstance::AppInstance() {
    glfwSetErrorCallback(electronGlfwError);
    if (!glfwInit()) {
        throw std::runtime_error("cannot initialize glfw!");
    }

    this->configMap = json_t::parse(std::fstream("config.json")); // config needs to be initialized ASAP
    this->isNativeWindow = configMap["ViewportMethod"] == "native-window";

    glfwDefaultWindowHints();
    glfwWindowHint(GLFW_RESIZABLE, isNativeWindow);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_VISIBLE, isNativeWindow);

    this->displayHandle = glfwCreateWindow(isNativeWindow ? 1280 : 2, isNativeWindow ? 720 : 2, "Electron", nullptr, nullptr);
    if (this->displayHandle == nullptr) {
        throw std::runtime_error("cannot instantiate window!");
    }
    glfwMakeContextCurrent(this->displayHandle);
    glfwSwapInterval(1);

    glewExperimental = GL_TRUE;
    GLenum glInitStatus = glewInit();
    if (glInitStatus != GLEW_OK) {
        throw std::runtime_error("cannot initialize glew! (" + std::string((const char*) glewGetErrorString(glInitStatus)) + ")");
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    if (!isNativeWindow) io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigWindowsMoveFromTitleBarOnly = true;
    io.WantSaveIniSettings = false;

    ImGui::StyleColorsLight();
    /* 
    ImGuiStyle& style = ImGui::GetStyle();
    style.Colors[ImGuiCol_TitleBg] = ImVec4(0.1f, 0.1f, 0.1f, 1);
    style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.1f, 0.1f, 0.1f, 1);
    style.Colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.08f, 0.08f, 0.08f, 1);
    style.Colors[ImGuiCol_MenuBarBg] = ImVec4(0.1f, 0.1f, 0.1f, 1);
    */

    io.Fonts->AddFontFromMemoryCompressedTTF(ELECTRON_FONT_compressed_data, ELECTRON_FONT_compressed_size, 16.0f);
    this->largeFont = io.Fonts->AddFontFromMemoryCompressedTTF(ELECTRON_FONT_compressed_data, ELECTRON_FONT_compressed_size, 32.0f);

    ImGui_ImplGlfw_InitForOpenGL(this->displayHandle, true);
    ImGui_ImplOpenGL3_Init();
    ImGui::StyleColorsDark();

    this->localizationMap = json_t::parse(std::fstream("localization_en.json"));
    this->projectOpened = false;

    this->graphics.ResizeRenderBuffer(128, 128);
    this->shortcuts.owner = this;

}

void Electron::AppInstance::Run() {
    while (!glfwWindowShouldClose(this->displayHandle)) {
        ImGuiIO& io = ImGui::GetIO();
        if (projectOpened) {
            project.SaveProject();
        }

        std::ofstream configStream("config.json");
        configStream << configMap.dump();

        PixelBuffer::filtering = configMap["TextureFiltering"] == "linear" ? GL_LINEAR : GL_NEAREST;

        if (isNativeWindow) {
            int displayWidth, displayHeight;
            glfwGetWindowSize(this->displayHandle, &displayWidth, &displayHeight);
            glViewport(0, 0, displayWidth, displayHeight);
        }

        glfwPollEvents();
        glClearColor(0, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT);

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        int windowIndex = 0;
        int destroyWindowTarget = -1;
        for (auto& window : this->content) {
            bool exitEditor = false;
            try {
                window->Render(this);
            } catch (ElectronSignal signal) {
                ExecuteSignal(signal, windowIndex, destroyWindowTarget, exitEditor);
                if (exitEditor) goto editor_end;
            }
            windowIndex++;
        }
        if (destroyWindowTarget != -1) {
            delete this->content[destroyWindowTarget];
            this->content.erase(content.begin() + destroyWindowTarget);
        }
        ImGui::Render();
        ImGui::EndFrame();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
        }
        glfwMakeContextCurrent(this->displayHandle);
        glfwSwapBuffers(this->displayHandle);
    }
    editor_end:

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwTerminate();
}

void Electron::AppInstance::ExecuteSignal(ElectronSignal signal, int windowIndex, int& destroyWindowTarget, bool& exitEditor) {
    switch (signal) {
        case ElectronSignal_CloseEditor: {
            exitEditor = true;
            break;
        }
        case ElectronSignal_CloseWindow: {
            destroyWindowTarget = windowIndex;
            break;
        }
        case ElectronSignal_SpawnRenderPreview: {
            AddUIContent(new RenderPreview());
            break;
        }
        case ElectronSignal_A: {
            print("Signal A");
            break;
        }
        case ElectronSignal_B: {
            print("Signal B");
            break;
        }
    }
}

void Electron::AppInstance::ExecuteSignal(ElectronSignal signal) {
    bool boolRef = false;
    int intRef = 0;
    ExecuteSignal(signal, 0, intRef, boolRef);
}

Electron::ElectronVector2f Electron::AppInstance::GetNativeWindowSize() {
    int width, height;
    glfwGetWindowSize(this->displayHandle, &width, &height);
    return Electron::ElectronVector2f{width, height};
}

Electron::ElectronVector2f Electron::AppInstance::GetNativeWindowPos() {
    int x, y;
    glfwGetWindowPos(this->displayHandle, &x, &y);
    return Electron::ElectronVector2f{x, y};
}

void Electron::ProjectMap::SaveProject() {
    std::string propertiesDump = propertiesMap.dump();

    std::ofstream propertiesStream(path + "/project.json");
    propertiesStream << propertiesDump;
}