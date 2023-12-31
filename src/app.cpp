#include "app.h"

#define GLAD_GLES2_IMPLEMENTATION
#include "gles2.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"


namespace Electron {

    ImFont* AppInstance::largeFont = nullptr;
    ImFont* AppInstance::mediumFont = nullptr;

    int UICounters::ProjectConfigurationCounter = 0;
    int UICounters::RenderPreviewCounter = 0;
    int UICounters::LayerPropertiesCounter = 0;
    int UICounters::AssetManagerCounter = 0;
    int UICounters::TimelineCounter = 0;
    int UICounters::AssetExaminerCounter;

namespace UI {
void Begin(const char *name, Signal signal,
           ImGuiWindowFlags flags) {
    if (signal == Signal::_None) {
        ImGui::Begin(name, nullptr, flags);
    } else {
        bool pOpen = true;
        ImGui::Begin(name, &pOpen, flags);
        if (!pOpen) {
            if (signal == Signal::_CloseWindow) {
                ImGui::End();
            }
            throw signal;
        }
    }
}

void End() { ImGui::End(); }
}


AppInstance::AppInstance() {
    this->showBadConfigMessage = false;
    this->ffmpegAvailable = false;
    Shared::app = this;
    Shared::graphics = new GraphicsCore();
    Shared::assets = new AssetRegistry();
    if (system("ffmpeg &> /dev/null") == 0 && system("ffprobe &> /dev/null") == 0) {
        ffmpegAvailable = true;
    }
    try {
        Shared::configMap = json_t::parse(
            std::fstream("misc/config.json")); // config needs to be initialized ASAP
    } catch (json_t::parse_error &ex) {
        RestoreBadConfig();
    }

    Cache::cacheIndex = JSON_AS_TYPE(Shared::configMap["CacheIndex"], int);

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        throw std::runtime_error("cannot initialize SDL2!");
    }
    SDL_version ver;
    SDL_GetVersion(&ver);
    print("SDL version: " << std::to_string(ver.major) << "." << std::to_string(ver.minor) << " " << std::to_string(ver.patch));

    float uiScaling = JSON_AS_TYPE(Shared::configMap["UIScaling"], float);
    this->isNativeWindow = (Shared::configMap["ViewportMethod"] == "native-window");

    std::vector<int> maybeSize = {
        JSON_AS_TYPE(Shared::configMap["LastWindowSize"].at(0), int),
        JSON_AS_TYPE(Shared::configMap["LastWindowSize"].at(1), int)};
    

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_ACCELERATED_VISUAL, 1);

    this->displayHandle = SDL_CreateWindow(
        "Electron", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        maybeSize[0], maybeSize[1], SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN | SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_RESIZABLE
    );

    SDL_GL_SetSwapInterval(1);

    if (this->displayHandle == nullptr) {
        throw std::runtime_error("cannot instantiate window!");
    }
    SDL_SetWindowMinimumSize(this->displayHandle, 640, 480);

    this->glc = SDL_GL_CreateContext(this->displayHandle);
    if (glc == nullptr) {
        throw std::runtime_error("cannot create gl context!");
    }
    SDL_GL_MakeCurrent(this->displayHandle, this->glc);
    this->rdr = SDL_CreateRenderer(this->displayHandle, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_TARGETTEXTURE | SDL_RENDERER_PRESENTVSYNC);

    gladLoadGLES2((GLADloadfunc) SDL_GL_GetProcAddress);

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
    this->mediumFont = io.Fonts->AddFontFromMemoryCompressedTTF(
        ELECTRON_FONT_compressed_data, ELECTRON_FONT_compressed_size,
        fontSize * 1.3f, NULL, io.Fonts->GetGlyphRangesCyrillic());
    icons_config.MergeMode = true;
    icons_config.PixelSnapH = true;
    icons_config.GlyphMinAdvanceX = fontSize * 2.0f / 3.0f;
    io.Fonts->AddFontFromFileTTF("misc/fa.ttf", fontSize * 1.3f * 2.0f / 3.0f,
                                    &icons_config, icons_ranges);

    ImGui_ImplSDL2_InitForOpenGL(this->displayHandle, &this->glc);
    ImGui_ImplOpenGL3_Init();
    DUMP_VAR(io.BackendRendererName);

    Shared::localizationMap = json_t::parse(std::fstream("misc/localization_en.json"));
    this->projectOpened = false;

    Shared::graphics->PrecompileEssentialShaders();
    Servers::InitializeCurl();

    Shared::graphics->ResizeRenderBuffer(128, 128);

    Shared::graphics->FetchAllLayers();

    // graphics.AddRenderLayer(RenderLayer("sdf2d_layer"));
    Shared::shadowTex = PixelBuffer("misc/shadow.png").BuildGPUTexture();
}

AppInstance::~AppInstance() {
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
    this->running = true;
    SDL_Event event;
    while (running) {
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL2_ProcessEvent(&event);
            switch (event.type) {
                case SDL_QUIT: {
                    running = false;
                    break;
                }
                case SDL_WINDOWEVENT: {
                    glViewport(0, 0, event.window.data1, event.window.data2);
                    break;
                }
            }
        }
        double firstTime = GetTime();
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

        Servers::AsyncWriterRequest({
            {"action", "write"},
            {"path", "misc/config.json"},
            {"content", Shared::configMap.dump()}
        });

        if (JSON_AS_TYPE(Shared::configMap["LastProject"], std::string) != "null") {
            if (!file_exists(
                    JSON_AS_TYPE(Shared::configMap["LastProject"], std::string))) {
                Shared::configMap["LastProject"] = "null";
            }
        }
        int width, height;
        SDL_GetWindowSize(this->displayHandle, &width, &height);
        Shared::configMap["LastWindowSize"] = {width, height};

        if (projectOpened) {
            Shared::configMap["LastProject"] = Shared::project.path;
        }
        Shared::configMap["CacheIndex"] = Cache::cacheIndex;

        int renderLengthCandidate = 0;
        for (auto &layer : Shared::graphics->layers) {
            renderLengthCandidate =
                glm::max(renderLengthCandidate, layer.endFrame);

            layer.beginFrame = glm::clamp(layer.beginFrame, 0, layer.endFrame);
        }
        Shared::graphics->renderLength = renderLengthCandidate;
        Shared::graphics->renderFrame = glm::clamp((float)Shared::graphics->renderFrame, 0.0f,
                                          (float)Shared::graphics->renderLength);

        PixelBuffer::filtering =
            Shared::configMap["TextureFiltering"] == "linear" ? GL_LINEAR : GL_NEAREST;
        if (projectOpened) {
            Shared::project.propertiesMap["LastSelectedLayer"] = Shared::selectedRenderLayer;
        }

        glClearColor(0, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT);

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
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
                    ImGui::BulletText("%s %s | %s", TextureUnion::GetIconByType(asset.type).c_str(), asset.name.c_str(), asset.path.c_str());
                }
                if (ButtonCenteredOnLine(ELECTRON_GET_LOCALIZATION("GENERIC_OK"))) {
                    Shared::assets->faultyAssets.clear();
                }
                ImGui::EndPopup();
            }
        }

        static bool warningOpen = true;
        std::string operationsPopupTag = string_format("%s##ffmpegActive", ELECTRON_GET_LOCALIZATION("FFMPEG_OPERATIONS_ARE_IN_PROGRESS"));
        if (!ffmpegAvailable) goto no_ffmpeg;
        
        /*
        if (assets.operations.size() != 0) {
            print("DUMP_OPERATIONS");
            for (auto& op : assets.operations) {
                DUMP_VAR(op.cmd);
                DUMP_VAR(op.info);
            }
        }
        */

        /* Execute completition procedures of AsyncFFMpegOperations */ {
            std::vector<int> deleteTargets{};
            for (auto& op : Shared::assets->operations) {
                if (op.Completed() && !op.procCompleted && op.proc != nullptr) {
                    op.proc(op.args);
                    op.procCompleted = true;
                    deleteTargets.push_back(op.id);
                }
            }
            for (auto& target : deleteTargets) {
                for (auto& op : Shared::assets->operations) {
                    if (op.id == target) {
                        Shared::assets->operations.erase(Shared::assets->operations.begin() + target);
                        break;
                    }
                }
            }
        }
        /* Fill previousProperties field */ {
            for (auto& layer : Shared::graphics->layers) {
                layer.previousProperties = layer.properties;
            }
        }
        /* Render UI */ {
            std::vector<UIActivity> uiCopy = this->content;
            for (auto &window : uiCopy) {
                bool exitEditor = false;
                try {
                    window.Render(this);
                } catch (Signal signal) {
                    ExecuteSignal(signal, windowIndex, destroyWindowTarget,
                                  exitEditor);
                    if (exitEditor)
                        goto editor_end;
                }
                windowIndex++;
            }
            for (auto &layer : Shared::graphics->layers) {
                layer.sortingProcedure(&layer);
            }
            if (destroyWindowTarget != -1) {
                *this->content[destroyWindowTarget].counter = 0;
                this->content.erase(content.begin() + destroyWindowTarget);
            }

            if (Shared::assets->operations.size() != 0) {
                AsyncFFMpegOperation* operation = nullptr;
                for (auto& op : Shared::assets->operations) {
                    if (!op.Completed() && !op.procCompleted) operation = &op;
                }
                if (operation != nullptr) {
                    ImGui::Begin("##operation_notification", nullptr, ImGuiWindowFlags_NoTitleBar 
                                                                    | ImGuiWindowFlags_NoResize 
                                                                    | ImGuiWindowFlags_NoScrollbar 
                                                                    | ImGuiWindowFlags_AlwaysAutoResize
                                                                    | ImGuiWindowFlags_NoFocusOnAppearing);
                        ImVec2 displaySize = GetNativeWindowSize();
                        ImVec2 windowSize = ImGui::GetWindowSize();

                        ImGui::SetWindowPos({
                           displaySize.x - windowSize.x - 30,
                            displaySize.y - windowSize.y - 30
                        });

                        ImGui::PushFont(mediumFont);
                            std::string info = operation->info;
                            ImGui::Text("%s", info.c_str());
                        ImGui::PopFont();
                    ImGui::End();
                }
            }
        }
        /* Trigger LayerOnPropertiesChange Event */ {
            for (auto& layer : Shared::graphics->layers) {
                if (layer.previousProperties != layer.properties) {
                    layer.onPropertiesChange(&layer);
                }
            }
        }
        goto render_success;
        no_ffmpeg: {
            RenderCriticalError(ELECTRON_GET_LOCALIZATION("FFMPEG_IS_NOT_AVAILABLE"), &warningOpen);
            if (!warningOpen) exit(1);
            goto render_success;
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
        SDL_GL_SwapWindow(this->displayHandle);

        if (projectOpened) {
            json_t assetRegistry = {};
            auto &_assets = Shared::assets->assets;
            for (int i = 0; i < _assets.size(); i++) {
                json_t assetDescription = {};
                TextureUnion texUnion = _assets[i];
                assetDescription["InternalName"] = texUnion.name;
                assetDescription["Path"] = texUnion.path;
                assetDescription["Type"] = texUnion.strType;
                assetDescription["ID"] = texUnion.id;
                assetDescription["AudioCoverPath"] = texUnion.audioCacheCover;
                if (texUnion.type == TextureUnionType::Audio) {
                    assetDescription["AudioCoverResolution"] = {
                        texUnion.coverResolution.x, texUnion.coverResolution.y
                    };
                }
                assetDescription["FFProbeData"] = texUnion.ffprobeData;
                assetRegistry.push_back(assetDescription);
            }
            Shared::project.propertiesMap["AssetRegistry"] = assetRegistry;
        }

        double secondTime = GetTime();
        Shared::deltaTime = secondTime - firstTime;
        float fps = 1.0 / Shared::deltaTime;
        SDL_SetWindowTitle(this->displayHandle, string_format("Electron [%0.1f FPS]", fps).c_str());
    }
editor_end:

    Terminate();
}

void AppInstance::Terminate() {

    Servers::DestroyCurl();
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
    Servers::AsyncWriterRequest({
        {"action", "kill"}
    });
    Servers::AudioServerRequest({
        {"action", "kill"}
    });

    SDL_GL_DeleteContext(glc);
    SDL_DestroyWindow(this->displayHandle);
    SDL_Quit();
}

void AppInstance::ExecuteSignal(Signal signal,
                                          int windowIndex,
                                          int &destroyWindowTarget,
                                          bool &exitEditor) {
    switch (signal) {
    case Signal::_CloseEditor: {
        exitEditor = true;
        break;
    }
    case Signal::_CloseWindow: {
        destroyWindowTarget = windowIndex;
        break;
    }
    case Signal::_SpawnRenderPreview: {
        AddUIContent(UIActivity("render_preview_impl", &UICounters::RenderPreviewCounter));
        break;
    }
    case Signal::_SpawnLayerProperties: {
        AddUIContent(UIActivity("layer_properties_impl", &UICounters::LayerPropertiesCounter));
        break;
    }
    case Signal::_SpawnAssetManager: {
        AddUIContent(UIActivity("asset_manager_impl", &UICounters::AssetManagerCounter));
        break;
    }
    case Signal::_SpawnTimeline: {
        AddUIContent(UIActivity("timeline_impl", &UICounters::TimelineCounter));
        break;
    }
    case Signal::_SpawnDockspace: {
        AddUIContent(UIActivity("dockspace_impl", nullptr));
        break;
    }
    case Signal::_SpawnProjectConfiguration: {
        AddUIContent(UIActivity("project_configuration_impl", &UICounters::ProjectConfigurationCounter));
        break;
    }
    case Signal::_SpawnAssetExaminer: {
        AddUIContent(UIActivity("asset_examiner_impl", &UICounters::AssetExaminerCounter));
        break;
    }
    case Signal::_A: {
        print("Signal A");
        break;
    }
    case Signal::_B: {
        print("Signal B");
        break;
    }

    default: {
        throw signal;
    }
    }
}

void AppInstance::ExecuteSignal(Signal signal) {
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
    SDL_GetWindowSize(this->displayHandle, &width, &height);
    return ImVec2{(float) width, (float) height};
}

ImVec2 AppInstance::GetNativeWindowPos() {
    int x, y;
    SDL_GetWindowPosition(this->displayHandle, &x, &y);
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

double AppInstance::GetTime() {
    return (double) SDL_GetTicks64() / 1000.0;
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
