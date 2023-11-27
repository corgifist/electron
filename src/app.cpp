#include "app.h"

#define GLAD_GLES2_IMPLEMENTATION
#include "gles2.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

namespace Electron {

    ImFont* AppInstance::largeFont = nullptr;

namespace UI {
void RenderDropShadow(ImTextureID tex_id, float size, ImU8 opacity) {
    ImVec2 p = ImGui::GetWindowPos();
    ImVec2 s = ImGui::GetWindowSize();
    ImVec2 m = {p.x + s.x, p.y + s.y};
    float uv0 = 0.0f;      // left/top region
    float uv1 = 0.333333f; // leftward/upper region
    float uv2 = 0.666666f; // rightward/lower region
    float uv3 = 1.0f;      // right/bottom region
    ImU32 col = (opacity << 24) | 0xFFFFFF;
    ImDrawList *dl = ImGui::GetWindowDrawList();
    dl->PushClipRectFullScreen();
    dl->AddImage(tex_id, {p.x - size, p.y - size}, {p.x, p.y}, {uv0, uv0},
                 {uv1, uv1}, col);
    dl->AddImage(tex_id, {p.x, p.y - size}, {m.x, p.y}, {uv1, uv0}, {uv2, uv1},
                 col);
    dl->AddImage(tex_id, {m.x, p.y - size}, {m.x + size, p.y}, {uv2, uv0},
                 {uv3, uv1}, col);
    dl->AddImage(tex_id, {p.x - size, p.y}, {p.x, m.y}, {uv0, uv1}, {uv1, uv2},
                 col);
    dl->AddImage(tex_id, {m.x, p.y}, {m.x + size, m.y}, {uv2, uv1}, {uv3, uv2},
                 col);
    dl->AddImage(tex_id, {p.x - size, m.y}, {p.x, m.y + size}, {uv0, uv2},
                 {uv1, uv3}, col);
    dl->AddImage(tex_id, {p.x, m.y}, {m.x, m.y + size}, {uv1, uv2}, {uv2, uv3},
                 col);
    dl->AddImage(tex_id, {m.x, m.y}, {m.x + size, m.y + size}, {uv2, uv2},
                 {uv3, uv3}, col);
    dl->PopClipRect();
}

void DropShadow() {
    RenderDropShadow((ImTextureID)(uint64_t) AppInstance::shadowTex, 24.0f,
                         100);
}

void Begin(const char *name, ElectronSignal signal,
           ImGuiWindowFlags flags) {
    if (signal == ElectronSignal_None) {
        ImGui::Begin(name, nullptr, flags);
        DropShadow();
    } else {
        bool pOpen = true;
        ImGui::Begin(name, &pOpen, flags);
        DropShadow();
        if (!pOpen) {
            if (signal == ElectronSignal_CloseWindow) {
                ImGui::End();
            }
            throw signal;
        }
    }
}

void End() { ImGui::End(); }
} // namespace UI

static void electronGlfwError(int id, const char *description) {
    print("GLFW_ERROR_ID: " << id);
    print("GLFW_ERROR: " << description);
}

GLuint AppInstance::shadowTex = 0;
AppInstance::AppInstance() {
    this->showBadConfigMessage = false;
    this->ffmpegAvailable = false;
    Shared::app = this;
    Shared::graphics = &graphics;
    Shared::assets = &assets;
    if (system("ffmpeg -h > /dev/null") == 0 && system("ffprobe -h > /dev/null") == 0) {
        ffmpegAvailable = true;
    }
    try {
        Shared::configMap = json_t::parse(
            std::fstream("config.json")); // config needs to be initialized ASAP
    } catch (json_t::parse_error &ex) {
        RestoreBadConfig();
    }

    Cache::cacheIndex = JSON_AS_TYPE(Shared::configMap["CacheIndex"], int);

    #ifdef __linux__
        std::string sessionType = getEnvVar("XDG_SESSION_TYPE");
        if (JSON_AS_TYPE(Shared::configMap["PreferX11"], bool) == true) {
            sessionType = "x11";
        }
        if (sessionType == "wayland") {
            glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_WAYLAND);
            print("(linux-only) using wayland platform");
        } else {
            glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_X11);
            print("(linux-only) fallback to x11 platform");
        }
    #endif
    glfwSetErrorCallback(electronGlfwError);
    if (!glfwInit()) {
        throw std::runtime_error("cannot initialize glfw!");
    }
    int major, minor, rev;
    glfwGetVersion(&major, &minor, &rev);
    print("GLFW version: " << major << "." << minor << " " << rev);

    float uiScaling = JSON_AS_TYPE(Shared::configMap["UIScaling"], float);
    this->isNativeWindow = (Shared::configMap["ViewportMethod"] == "native-window");

    glfwDefaultWindowHints();
    glfwWindowHint(GLFW_RESIZABLE, isNativeWindow);
    glfwWindowHint(GLFW_VISIBLE, isNativeWindow);

    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
    glfwWindowHint(GLFW_CONTEXT_CREATION_API, GLFW_EGL_CONTEXT_API);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);

    std::vector<int> maybeSize = {
        JSON_AS_TYPE(Shared::configMap["LastWindowSize"].at(0), int),
        JSON_AS_TYPE(Shared::configMap["LastWindowSize"].at(1), int)};
    this->displayHandle = glfwCreateWindow(isNativeWindow ? maybeSize[0] : 8,
                                           isNativeWindow ? maybeSize[1] : 8,
                                           "Electron", nullptr, nullptr);
    if (this->displayHandle == nullptr) {
        throw std::runtime_error("cannot instantiate window!");
    }

    glfwMakeContextCurrent(this->displayHandle);
    glfwSwapInterval(1);

    gladLoadGLES2(glfwGetProcAddress);

    this->renderer = std::string((const char*) glGetString(GL_RENDERER));
    this->vendor = std::string((const char*) glGetString(GL_VENDOR));
    this->version = std::string((const char*) glGetString(GL_VERSION));
    DUMP_VAR(this->renderer);
    DUMP_VAR(this->vendor);
    DUMP_VAR(this->version);

    std::ostringstream oss;
    oss << glGetString(GL_VENDOR);
    if (oss.str().find("NVIDIA") != std::string::npos) {
        Wavefront::x = 8;
        Wavefront::y = 4;
        Wavefront::z = 1;
        print("Applied NVIDIA Wavefront optimization");
    } else if (oss.str().find("AMD") != std::string::npos) {
        Wavefront::x = 8;
        Wavefront::y = 8;
        Wavefront::z = 1;
        print("Applied AMD Wavefront optimization");
    } else {
        Wavefront::x = 1;
        Wavefront::y = 1;
        Wavefront::z = 1;
        print("Unknown vendor detected, switching to default wavefront");
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    if (!isNativeWindow)
        io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DpiEnableScaleFonts;
    io.ConfigFlags |= ImGuiConfigFlags_DpiEnableScaleViewports;
    io.ConfigWindowsMoveFromTitleBarOnly = true;
    io.WantSaveIniSettings = true;
    this->context = ImGui::GetCurrentContext();

    SetupImGuiStyle();

    this->fontSize = 16.0f * uiScaling;
    io.Fonts->AddFontFromMemoryCompressedTTF(
        ELECTRON_FONT_compressed_data, ELECTRON_FONT_compressed_size, fontSize,
        NULL, io.Fonts->GetGlyphRangesCyrillic());
    static const ImWchar icons_ranges[] = {ICON_MIN_FA, ICON_MAX_16_FA, 0};
    ImFontConfig icons_config;
    icons_config.MergeMode = true;
    icons_config.PixelSnapH = true;
    icons_config.GlyphMinAdvanceX = fontSize * 2.0f / 3.0f;
    io.Fonts->AddFontFromFileTTF("misc/fa.ttf", fontSize * 2.0f / 3.0f,
                                 &icons_config, icons_ranges);
    this->largeFont = io.Fonts->AddFontFromMemoryCompressedTTF(
        ELECTRON_FONT_compressed_data, ELECTRON_FONT_compressed_size,
        fontSize * 2.0f, NULL, io.Fonts->GetGlyphRangesCyrillic());
    icons_config.MergeMode = true;
    icons_config.PixelSnapH = true;
    icons_config.GlyphMinAdvanceX = fontSize * 2.0f / 3.0f;
    io.Fonts->AddFontFromFileTTF("misc/fa.ttf", fontSize * 2.0f * 2.0f / 3.0f,
                                 &icons_config, icons_ranges);

    ImGui_ImplGlfw_InitForOpenGL(this->displayHandle, true);
    ImGui_ImplOpenGL3_Init();
    DUMP_VAR(io.BackendRendererName);

    Shared::localizationMap = json_t::parse(std::fstream("localization_en.json"));
    this->projectOpened = false;

    this->graphics.PrecompileEssentialShaders();
    Servers::InitializeCurl();

    this->graphics.ResizeRenderBuffer(128, 128);

    this->graphics.FetchAllLayers();

    // graphics.AddRenderLayer(RenderLayer("sdf2d_layer"));
    AppInstance::shadowTex = PixelBuffer("misc/shadow.png").BuildGPUTexture();
}

AppInstance::~AppInstance() {
    for (auto ui : this->content) {
        delete ui;
    }
    this->content.clear();
}

static bool showDemoWindow = false;

static void ChangeShowDemoWindow() { showDemoWindow = !showDemoWindow; }

void AppInstance::RenderCriticalError(std::string text, bool* p_open) {
    if (*p_open == false) return;
    std::string popupTitle = (std::string(ELECTRON_GET_LOCALIZATION("CRITICAL_ERROR")) + "##" + std::string(std::to_string((uint64_t) p_open))).c_str();
    ImGui::OpenPopup(popupTitle.c_str());

    if (ImGui::BeginPopupModal(popupTitle.c_str(), p_open, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove)) {
        ImGui::SetWindowPos({GetNativeWindowSize().x / 2.0f - ImGui::GetWindowSize().x / 2.0f, GetNativeWindowSize().y / 2.0f - ImGui::GetWindowSize().y / 2.0f});
        ImGui::Text("%s", text.c_str());
        ImGui::EndPopup();
    }
}

void AppInstance::Run() {
    while (!glfwWindowShouldClose(this->displayHandle)) {
        this->running = true;
        this->context = ImGui::GetCurrentContext();   
        static bool audioServerDead = false;
        if (!Servers::AudioServerRequest({
            {"action", "alive"}
        }).alive && !audioServerDead) {
            audioServerDead = true;
        }

        static bool asyncWriterDead = false;
        if (!Servers::AsyncWriterRequest({
            {"action", "alive"}
        }).alive && !asyncWriterDead) {
            asyncWriterDead = true;
        }


        ImGuiIO &io = ImGui::GetIO();
        if (projectOpened) {
            Shared::project.SaveProject();
        }

        // std::ofstream configStream("config.json");
        // configStream << configMap.dump();

        Servers::AsyncWriterRequest({
            {"action", "write"},
            {"path", "config.json"},
            {"content", Shared::configMap.dump()}
        });

        if (JSON_AS_TYPE(Shared::configMap["LastProject"], std::string) != "null") {
            if (!file_exists(
                    JSON_AS_TYPE(Shared::configMap["LastProject"], std::string))) {
                Shared::configMap["LastProject"] = "null";
            }
        }
        int width, height;
        glfwGetWindowSize(displayHandle, &width, &height);
        Shared::configMap["LastWindowSize"] = {width, height};

        if (projectOpened) {
            Shared::configMap["LastProject"] = Shared::project.path;
        }
        Shared::configMap["CacheIndex"] = Cache::cacheIndex;

        int renderLengthCandidate = 0;
        for (auto &layer : graphics.layers) {
            renderLengthCandidate =
                glm::max(renderLengthCandidate, layer.endFrame);

            layer.beginFrame = glm::clamp(layer.beginFrame, 0, layer.endFrame);
        }
        graphics.renderLength = renderLengthCandidate;
        graphics.renderFrame = glm::clamp((float)graphics.renderFrame, 0.0f,
                                          (float)graphics.renderLength);

        PixelBuffer::filtering =
            Shared::configMap["TextureFiltering"] == "linear" ? GL_LINEAR : GL_NEAREST;
        if (projectOpened) {
            Shared::project.propertiesMap["LastSelectedLayer"] = Shared::selectedRenderLayer;
        }

        if (isNativeWindow) {
            int displayWidth, displayHeight;
            glfwGetWindowSize(this->displayHandle, &displayWidth,
                              &displayHeight);
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
        if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_I)) {
            ChangeShowDemoWindow();
        }
        if (showDemoWindow) {
            ImGui::ShowDemoWindow();
        }

        if (showBadConfigMessage) {
            RenderCriticalError(ELECTRON_GET_LOCALIZATION("CORRUPTED_CONFIG_MESSAGE_MESSAGE"), &showBadConfigMessage);
        }


        if (audioServerDead) {
            static bool asOpen = true;
            RenderCriticalError(ELECTRON_GET_LOCALIZATION("AUDIO_SERVER_CRASHED"), &asOpen);
        }

        if (asyncWriterDead) {
            static bool awOpen = true;
            RenderCriticalError(ELECTRON_GET_LOCALIZATION("ASYNC_WRITER_CRASHED"), &awOpen);
        }

        if (Shared::assets->faultyAssets.size() != 0) {
            ImGui::OpenPopup(ELECTRON_GET_LOCALIZATION("CRITICAL_ERROR"));
            if (ImGui::BeginPopupModal(ELECTRON_GET_LOCALIZATION("CRITICAL_ERROR"), nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize)) {
                ImGui::SetWindowPos({GetNativeWindowSize().x / 2.0f - ImGui::GetWindowSize().x / 2.0f, GetNativeWindowSize().y / 2.0f - ImGui::GetWindowSize().y / 2.0f});
                ImGui::Text("%s:", ELECTRON_GET_LOCALIZATION("SOME_ASSETS_ARE_CORRUPTED"));
                for (auto& asset : Shared::assets->faultyAssets) {
                    ImGui::BulletText("%s %s | %s", TextureUnion::GetIconByType(asset.type), asset.name.c_str(), asset.path.c_str());
                }
                if (ButtonCenteredOnLine(ELECTRON_GET_LOCALIZATION("GENERIC_OK"))) {
                    Shared::assets->faultyAssets.clear();
                }
                ImGui::EndPopup();
            }
        }

        std::vector<ElectronUI *> uiCopy = this->content;
        static bool warningOpen = true;
        std::string operationsPopupTag = string_format("%s##ffmpegActive", ELECTRON_GET_LOCALIZATION("FFMPEG_OPERATIONS_ARE_IN_PROGRESS"));
        AsyncFFMpegOperation* pOperation = nullptr;
        if (!ffmpegAvailable) goto no_ffmpeg;
        // Execute competition procedures of AsyncFFMpegOperations
        for (auto& op : assets.operations) {
            if (op.Completed() && !op.procCompleted && op.proc != nullptr) {
                op.proc(op.args);
                op.procCompleted = true;
            }
        }
        // Show modal dialogs while performing AsyncFFMpegOperations
        if (assets.operations.size() > 0) {
            for (auto& op : assets.operations) {
                if (!op.Completed()) {
                    pOperation = &op;
                    goto ffmpeg_active_operations;
                }
            }
        }
        // Render UI
        for (auto &window : uiCopy) {
            bool exitEditor = false;
            try {
                window->Render(this);
            } catch (ElectronSignal signal) {
                ExecuteSignal(signal, windowIndex, destroyWindowTarget,
                              exitEditor);
                if (exitEditor)
                    goto editor_end;
            }
            windowIndex++;
        }
        for (auto &layer : graphics.layers) {
            layer.sortingProcedure(&layer);
        }
        if (destroyWindowTarget != -1) {
            delete this->content[destroyWindowTarget];
            this->content.erase(content.begin() + destroyWindowTarget);
        }
        goto render_success;
        no_ffmpeg:
        RenderCriticalError(ELECTRON_GET_LOCALIZATION("FFMPEG_IS_NOT_AVAILABLE"), &warningOpen);
        if (!warningOpen) exit(1);
        goto render_success;

        ffmpeg_active_operations:
        ImGui::OpenPopup(operationsPopupTag.c_str());
        if (ImGui::BeginPopupModal(operationsPopupTag.c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoResize)) {
            ImGui::Text("%s", pOperation->Cmd().c_str());
            ImGui::EndPopup();
        }


        goto render_success;

        render_success:
        ImGui::Render();
        ImGui::EndFrame();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
        }
        glfwMakeContextCurrent(this->displayHandle);
        glfwSwapBuffers(this->displayHandle);

        if (projectOpened) {
            json_t assetRegistry = {};
            auto &_assets = this->assets.assets;
            for (int i = 0; i < _assets.size(); i++) {
                json_t assetDescription = {};
                TextureUnion texUnion = _assets[i];
                assetDescription["InternalName"] = texUnion.name;
                assetDescription["Path"] = texUnion.path;
                assetDescription["Type"] = texUnion.strType;
                assetDescription["ID"] = texUnion.id;
                assetDescription["AudioCoverPath"] = texUnion.audioCacheCover;
                if (texUnion.type == TextureUnionType::Audio) {
                    assetDescription["AudioProbeData"] = std::get<AudioMetadata>(texUnion.as).probe;
                    assetDescription["AudioCoverResolution"] = {
                        texUnion.coverResolution.x, texUnion.coverResolution.y
                    };
                }
                assetRegistry.push_back(assetDescription);
            }
            Shared::project.propertiesMap["AssetRegistry"] = assetRegistry;
        }
    }
editor_end:

    Terminate();
}

void AppInstance::Terminate() {
    this->running = false;
    Servers::AsyncWriterRequest({
        {"action", "kill"}
    });
    Servers::AudioServerRequest({
        {"action", "kill"}
    });

    Servers::DestroyCurl();
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}

void AppInstance::ExecuteSignal(ElectronSignal signal,
                                          int windowIndex,
                                          int &destroyWindowTarget,
                                          bool &exitEditor) {
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
    case ElectronSignal_SpawnLayerProperties: {
        AddUIContent(new LayerProperties());
        break;
    }
    case ElectronSignal_SpawnAssetManager: {
        AddUIContent(new AssetManager());
        break;
    }
    case ElectronSignal_SpawnTimeline: {
        AddUIContent(new Timeline());
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

    default: {
        throw signal;
    }
    }
}

void AppInstance::ExecuteSignal(ElectronSignal signal) {
    bool boolRef = false;
    int intRef = 0;
    ExecuteSignal(signal, 0, intRef, boolRef);
}

void AppInstance::RestoreBadConfig() {
    Shared::configMap["RenderPreviewTimelinePrescision"] = 2;
    Shared::configMap["ResizeInterpolation"] = true;
    Shared::configMap["TextureFiltering"] = "nearest";
    Shared::configMap["ViewportMethod"] = "native-window";
    Shared::configMap["UIScaling"] = 1.0f;
    Shared::configMap["LastProject"] = "null";
    Shared::configMap["LastWindowSize"] = {1280, 720};

    this->showBadConfigMessage = true;
}

ImVec2 AppInstance::GetNativeWindowSize() {
    int width, height;
    glfwGetWindowSize(this->displayHandle, &width, &height);
    return ImVec2{(float) width, (float) height};
}

ImVec2 AppInstance::GetNativeWindowPos() {
    int x, y;
    glfwGetWindowPos(this->displayHandle, &x, &y);
    return ImVec2{(float) x, (float) y};
}

bool AppInstance::ButtonCenteredOnLine(const char *label,
                                                 float alignment) {
    ImGuiStyle &style = ImGui::GetStyle();

    float size = ImGui::CalcTextSize(label).x + style.FramePadding.x * 2.0f;
    float avail = ImGui::GetContentRegionAvail().x;

    float off = (avail - size) * alignment;
    if (off > 0.0f)
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + off);

    return ImGui::Button(label);
}

void AppInstance::SetupImGuiStyle() {

    // Unreal style by dev0-1 from ImThemes
    ImVec4 *colors = ImGui::GetStyle().Colors;
    colors[ImGuiCol_Text] = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
    colors[ImGuiCol_TextDisabled] = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
    colors[ImGuiCol_WindowBg] = ImVec4(0.06f, 0.06f, 0.06f, 1.0f);
    colors[ImGuiCol_ChildBg] = ImVec4(1.00f, 1.00f, 1.00f, 0.00f);
    colors[ImGuiCol_PopupBg] = ImVec4(0.08f, 0.08f, 0.08f, 0.94f);
    colors[ImGuiCol_Border] = ImVec4(0.20f, 0.20f, 0.20f, 0.50f);
    colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_FrameBg] = ImVec4(0.20f, 0.21f, 0.22f, 0.54f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.40f, 0.40f, 0.40f, 0.40f);
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.18f, 0.18f, 0.18f, 0.67f);
    colors[ImGuiCol_TitleBg] = ImVec4(0.04f, 0.04f, 0.04f, 1.00f);
    colors[ImGuiCol_TitleBgActive] = ImVec4(0.29f, 0.29f, 0.29f, 1.00f);
    colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.00f, 0.00f, 0.00f, 0.51f);
    colors[ImGuiCol_MenuBarBg] = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
    colors[ImGuiCol_ScrollbarBg] = ImVec4(0.02f, 0.02f, 0.02f, 0.53f);
    colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.31f, 0.31f, 0.31f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.41f, 0.41f, 0.41f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.51f, 0.51f, 0.51f, 1.00f);
    colors[ImGuiCol_CheckMark] = ImVec4(0.94f, 0.94f, 0.94f, 1.00f);
    colors[ImGuiCol_SliderGrab] = ImVec4(0.51f, 0.51f, 0.51f, 1.00f);
    colors[ImGuiCol_SliderGrabActive] = ImVec4(0.86f, 0.86f, 0.86f, 1.00f);
    colors[ImGuiCol_Button] = ImVec4(0.44f, 0.44f, 0.44f, 0.40f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.46f, 0.47f, 0.48f, 1.00f);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.42f, 0.42f, 0.42f, 1.00f);
    colors[ImGuiCol_Header] = ImVec4(0.70f, 0.70f, 0.70f, 0.31f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.70f, 0.70f, 0.70f, 0.80f);
    colors[ImGuiCol_HeaderActive] = ImVec4(0.48f, 0.50f, 0.52f, 1.00f);
    colors[ImGuiCol_Separator] = ImVec4(0.43f, 0.43f, 0.50f, 0.50f);
    colors[ImGuiCol_SeparatorHovered] = ImVec4(0.72f, 0.72f, 0.72f, 0.78f);
    colors[ImGuiCol_SeparatorActive] = ImVec4(0.51f, 0.51f, 0.51f, 1.00f);
    colors[ImGuiCol_ResizeGrip] = ImVec4(0.91f, 0.91f, 0.91f, 0.25f);
    colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.81f, 0.81f, 0.81f, 0.67f);
    colors[ImGuiCol_ResizeGripActive] = ImVec4(0.46f, 0.46f, 0.46f, 0.95f);
    colors[ImGuiCol_Tab] = ImVec4(0.34f, 0.34f, 0.34f, 0.86f);
    colors[ImGuiCol_TabHovered] = ImVec4(0.59f, 0.59f, 0.59f, 0.80f);
    colors[ImGuiCol_TabActive] = ImVec4(0.66f, 0.66f, 0.66f, 1.00f);
    colors[ImGuiCol_TabUnfocused] = ImVec4(0.09f, 0.09f, 0.09f, 0.97f);
    colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
    colors[ImGuiCol_DockingPreview] = ImVec4(0.62f, 0.62f, 0.62f, 0.70f);
    colors[ImGuiCol_DockingEmptyBg] = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
    colors[ImGuiCol_PlotLines] = ImVec4(0.61f, 0.61f, 0.61f, 1.00f);
    colors[ImGuiCol_PlotLinesHovered] = ImVec4(1.00f, 0.43f, 0.35f, 1.00f);
    colors[ImGuiCol_PlotHistogram] = ImVec4(0.73f, 0.60f, 0.15f, 1.00f);
    colors[ImGuiCol_PlotHistogramHovered] = ImVec4(1.00f, 0.60f, 0.00f, 1.00f);
    colors[ImGuiCol_TableHeaderBg] = ImVec4(0.19f, 0.19f, 0.20f, 1.00f);
    colors[ImGuiCol_TableBorderStrong] = ImVec4(0.31f, 0.31f, 0.35f, 1.00f);
    colors[ImGuiCol_TableBorderLight] = ImVec4(0.23f, 0.23f, 0.25f, 1.00f);
    colors[ImGuiCol_TableRowBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_TableRowBgAlt] = ImVec4(1.00f, 1.00f, 1.00f, 0.06f);
    colors[ImGuiCol_TextSelectedBg] = ImVec4(0.87f, 0.87f, 0.87f, 0.35f);
    colors[ImGuiCol_DragDropTarget] = ImVec4(1.00f, 1.00f, 1.00f, 0.90f);
    colors[ImGuiCol_NavHighlight] = ImVec4(0.60f, 0.60f, 0.60f, 1.00f);
    colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
    colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
    colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.35f);
    colors[ImGuiCol_TabActive] = ImVec4(0.44f, 0.44f, 0.44f, 1.00f);
    ImGuiStyle &style = ImGui::GetStyle();
    style.WindowRounding = 4;
    style.FrameRounding = 4;
    style.ChildRounding = 4;
    style.PopupRounding = 4;
    style.TabRounding = 4;
    style.ScrollbarRounding = 4;
    style.GrabRounding = 4;
    style.ScaleAllSizes(JSON_AS_TYPE(Shared::configMap["UIScaling"], float));
}
} // namespace Electron
