#include "app.h"

static void electronGlfwError(int id, const char* description) {
    print("GLFW_ERROR_ID: " << id);
    print("GLFW_ERROR: " << description);
}

Electron::AppInstance::AppInstance() {
    this->selectedRenderLayer = 0;
    this->showBadConfigMessage = false;

    glfwSetErrorCallback(electronGlfwError);
    if (!glfwInit()) {
        throw std::runtime_error("cannot initialize glfw!");
    }

    try {
        this->configMap = json_t::parse(std::fstream("config.json")); // config needs to be initialized ASAP
    } catch (json_t::parse_error& ex) {
        RestoreBadConfig();
    }
    float uiScaling = JSON_AS_TYPE(configMap["UIScaling"], float);
    this->isNativeWindow = configMap["ViewportMethod"] == "native-window";

    glfwDefaultWindowHints();
    glfwWindowHint(GLFW_RESIZABLE, isNativeWindow);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_VISIBLE, isNativeWindow);

    this->displayHandle = glfwCreateWindow(isNativeWindow ? 1280 : 8, isNativeWindow ? 720 : 8, "Electron", nullptr, nullptr);
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

    SetupImGuiStyle();

    io.Fonts->AddFontFromMemoryCompressedTTF(ELECTRON_FONT_compressed_data, ELECTRON_FONT_compressed_size, 16.0f * uiScaling);
    this->largeFont = io.Fonts->AddFontFromMemoryCompressedTTF(ELECTRON_FONT_compressed_data, ELECTRON_FONT_compressed_size, 32.0f * uiScaling);

    ImGui_ImplGlfw_InitForOpenGL(this->displayHandle, true);
    ImGui_ImplOpenGL3_Init();

    this->localizationMap = json_t::parse(std::fstream("localization_en.json"));
    this->projectOpened = false;

    this->graphics.ResizeRenderBuffer(128, 128);
    this->shortcuts.owner = this;


    InitializeDylibs();
}

Electron::AppInstance::~AppInstance() {
    for (auto ui : this->content) {
        delete ui;
    }
    this->content.clear();
}

void Electron::AppInstance::Run() {
    while (!glfwWindowShouldClose(this->displayHandle)) {
        ImGuiIO& io = ImGui::GetIO();
        if (projectOpened) {
            project.SaveProject();
        }

        std::ofstream configStream("config.json");
        configStream << configMap.dump();

        if (projectOpened) {
            configMap["LastProject"] = project.path;
        }

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
        if (showBadConfigMessage) {
            ImGui::Begin(ELECTRON_GET_LOCALIZATION(this, "CORRUPTED_CONFIG_MESSAGE_TITLE"), nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_AlwaysAutoResize);
                ImGui::FocusWindow(ImGui::GetCurrentWindow());
                std::string configMessage = ELECTRON_GET_LOCALIZATION(this, "CORRUPTED_CONFIG_MESSAGE_MESSAGE");
                ImVec2 messageSize = ImGui::CalcTextSize(configMessage.c_str());
                ImVec2 windowSize = ImGui::GetWindowSize();
                ImGui::SetCursorPosX(windowSize.x / 2.0f - messageSize.x / 2.0f);
                ImGui::Text(configMessage.c_str());

                ImGuiStyle& style = ImGui::GetStyle();
                std::string okString = "OK";
                ImVec2 okSize = ImGui::CalcTextSize(okString.c_str());
                if (ButtonCenteredOnLine(okString.c_str())) {
                    showBadConfigMessage = false;
                }
            ImGui::End();
        }

        std::vector<ElectronUI*> uiCopy = this->content;
        for (auto& window : uiCopy) {
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

    Terminate();
}

void Electron::AppInstance::Terminate() {
    DestroyDylibs();
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
        case ElectronSignal_SpawnLayerProperties: {
            AddUIContent(new LayerProperties());
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

void Electron::AppInstance::ExecuteSignal(ElectronSignal signal) {
    bool boolRef = false;
    int intRef = 0;
    ExecuteSignal(signal, 0, intRef, boolRef);
}

void Electron::AppInstance::RestoreBadConfig() {
    this->configMap["RenderPreviewTimelinePrescision"] = 2;
    this->configMap["ResizeInterpolation"] = true;
    this->configMap["TextureFiltering"] = "nearest";
    this->configMap["ViewportMethod"] = "native-window";
    this->configMap["UIScaling"] = 1.0f;
    this->configMap["LastProject"] = "null";

    this->showBadConfigMessage = true;
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

bool Electron::AppInstance::ButtonCenteredOnLine(const char* label, float alignment) {
    ImGuiStyle& style = ImGui::GetStyle();

    float size = ImGui::CalcTextSize(label).x + style.FramePadding.x * 2.0f;
    float avail = ImGui::GetContentRegionAvail().x;

    float off = (avail - size) * alignment;
    if (off > 0.0f)
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + off);

    return ImGui::Button(label);
}

void Electron::ProjectMap::SaveProject() {
    std::string propertiesDump = propertiesMap.dump();

    std::ofstream propertiesStream(path + "/project.json");
    propertiesStream << propertiesDump;
}

void Electron::AppInstance::SetupImGuiStyle()
{
	// Unreal style by dev0-1 from ImThemes
	ImGuiStyle& style = ImGui::GetStyle();
	
	style.Alpha = 1.0f;
	style.DisabledAlpha = 0.6000000238418579f;
	style.WindowPadding = ImVec2(8.0f, 8.0f);
	style.WindowRounding = 0.0f;
	style.WindowBorderSize = 1.0f;
	style.WindowMinSize = ImVec2(32.0f, 32.0f);
	style.WindowTitleAlign = ImVec2(0.0f, 0.5f);
	style.WindowMenuButtonPosition = ImGuiDir_Left;
	style.ChildRounding = 0.0f;
	style.ChildBorderSize = 1.0f;
	style.PopupRounding = 0.0f;
	style.PopupBorderSize = 1.0f;
	style.FramePadding = ImVec2(4.0f, 3.0f);
	style.FrameRounding = 0.0f;
	style.FrameBorderSize = 0.0f;
	style.ItemSpacing = ImVec2(8.0f, 4.0f);
	style.ItemInnerSpacing = ImVec2(4.0f, 4.0f);
	style.CellPadding = ImVec2(4.0f, 2.0f);
	style.IndentSpacing = 21.0f;
	style.ColumnsMinSpacing = 6.0f;
	style.ScrollbarSize = 14.0f;
	style.ScrollbarRounding = 9.0f;
	style.GrabMinSize = 10.0f;
	style.GrabRounding = 0.0f;
	style.TabRounding = 4.0f;
	style.TabBorderSize = 0.0f;
	style.TabMinWidthForCloseButton = 0.0f;
	style.ColorButtonPosition = ImGuiDir_Right;
	style.ButtonTextAlign = ImVec2(0.5f, 0.5f);
	style.SelectableTextAlign = ImVec2(0.0f, 0.0f);
	
	style.Colors[ImGuiCol_Text] = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
	style.Colors[ImGuiCol_TextDisabled] = ImVec4(0.4980392158031464f, 0.4980392158031464f, 0.4980392158031464f, 1.0f);
	style.Colors[ImGuiCol_WindowBg] = ImVec4(0.05882352963089943f, 0.05882352963089943f, 0.05882352963089943f, 0.9399999976158142f);
	style.Colors[ImGuiCol_ChildBg] = ImVec4(1.0f, 1.0f, 1.0f, 0.0f);
	style.Colors[ImGuiCol_PopupBg] = ImVec4(0.0784313753247261f, 0.0784313753247261f, 0.0784313753247261f, 0.9399999976158142f);
	style.Colors[ImGuiCol_Border] = ImVec4(0.4274509847164154f, 0.4274509847164154f, 0.4980392158031464f, 0.5f);
	style.Colors[ImGuiCol_BorderShadow] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
	style.Colors[ImGuiCol_FrameBg] = ImVec4(0.2000000029802322f, 0.2078431397676468f, 0.2196078449487686f, 0.5400000214576721f);
	style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.4000000059604645f, 0.4000000059604645f, 0.4000000059604645f, 0.4000000059604645f);
	style.Colors[ImGuiCol_FrameBgActive] = ImVec4(0.1764705926179886f, 0.1764705926179886f, 0.1764705926179886f, 0.6700000166893005f);
	style.Colors[ImGuiCol_TitleBg] = ImVec4(0.03921568766236305f, 0.03921568766236305f, 0.03921568766236305f, 1.0f);
	style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.2862745225429535f, 0.2862745225429535f, 0.2862745225429535f, 1.0f);
	style.Colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.0f, 0.0f, 0.0f, 0.5099999904632568f);
	style.Colors[ImGuiCol_MenuBarBg] = ImVec4(0.1372549086809158f, 0.1372549086809158f, 0.1372549086809158f, 1.0f);
	style.Colors[ImGuiCol_ScrollbarBg] = ImVec4(0.01960784383118153f, 0.01960784383118153f, 0.01960784383118153f, 0.5299999713897705f);
	style.Colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.3098039329051971f, 0.3098039329051971f, 0.3098039329051971f, 1.0f);
	style.Colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.407843142747879f, 0.407843142747879f, 0.407843142747879f, 1.0f);
	style.Colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.5098039507865906f, 0.5098039507865906f, 0.5098039507865906f, 1.0f);
	style.Colors[ImGuiCol_CheckMark] = ImVec4(0.9372549057006836f, 0.9372549057006836f, 0.9372549057006836f, 1.0f);
	style.Colors[ImGuiCol_SliderGrab] = ImVec4(0.5098039507865906f, 0.5098039507865906f, 0.5098039507865906f, 1.0f);
	style.Colors[ImGuiCol_SliderGrabActive] = ImVec4(0.8588235378265381f, 0.8588235378265381f, 0.8588235378265381f, 1.0f);
	style.Colors[ImGuiCol_Button] = ImVec4(0.4392156898975372f, 0.4392156898975372f, 0.4392156898975372f, 0.4000000059604645f);
	style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.4588235318660736f, 0.4666666686534882f, 0.47843137383461f, 1.0f);
	style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.4196078479290009f, 0.4196078479290009f, 0.4196078479290009f, 1.0f);
	style.Colors[ImGuiCol_Header] = ImVec4(0.6980392336845398f, 0.6980392336845398f, 0.6980392336845398f, 0.3100000023841858f);
	style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.6980392336845398f, 0.6980392336845398f, 0.6980392336845398f, 0.800000011920929f);
	style.Colors[ImGuiCol_HeaderActive] = ImVec4(0.47843137383461f, 0.4980392158031464f, 0.5176470875740051f, 1.0f);
	style.Colors[ImGuiCol_Separator] = ImVec4(0.4274509847164154f, 0.4274509847164154f, 0.4980392158031464f, 0.5f);
	style.Colors[ImGuiCol_SeparatorHovered] = ImVec4(0.7176470756530762f, 0.7176470756530762f, 0.7176470756530762f, 0.7799999713897705f);
	style.Colors[ImGuiCol_SeparatorActive] = ImVec4(0.5098039507865906f, 0.5098039507865906f, 0.5098039507865906f, 1.0f);
	style.Colors[ImGuiCol_ResizeGrip] = ImVec4(0.9098039269447327f, 0.9098039269447327f, 0.9098039269447327f, 0.25f);
	style.Colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.8078431487083435f, 0.8078431487083435f, 0.8078431487083435f, 0.6700000166893005f);
	style.Colors[ImGuiCol_ResizeGripActive] = ImVec4(0.4588235318660736f, 0.4588235318660736f, 0.4588235318660736f, 0.949999988079071f);
	style.Colors[ImGuiCol_Tab] = ImVec4(0.1764705926179886f, 0.3490196168422699f, 0.5764706134796143f, 0.8619999885559082f);
	style.Colors[ImGuiCol_TabHovered] = ImVec4(0.2588235437870026f, 0.5882353186607361f, 0.9764705896377563f, 0.800000011920929f);
	style.Colors[ImGuiCol_TabActive] = ImVec4(0.196078434586525f, 0.407843142747879f, 0.6784313917160034f, 1.0f);
	style.Colors[ImGuiCol_TabUnfocused] = ImVec4(0.06666667014360428f, 0.1019607856869698f, 0.1450980454683304f, 0.9724000096321106f);
	style.Colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.1333333402872086f, 0.2588235437870026f, 0.4235294163227081f, 1.0f);
	style.Colors[ImGuiCol_PlotLines] = ImVec4(0.6078431606292725f, 0.6078431606292725f, 0.6078431606292725f, 1.0f);
	style.Colors[ImGuiCol_PlotLinesHovered] = ImVec4(1.0f, 0.4274509847164154f, 0.3490196168422699f, 1.0f);
	style.Colors[ImGuiCol_PlotHistogram] = ImVec4(0.729411780834198f, 0.6000000238418579f, 0.1490196138620377f, 1.0f);
	style.Colors[ImGuiCol_PlotHistogramHovered] = ImVec4(1.0f, 0.6000000238418579f, 0.0f, 1.0f);
	style.Colors[ImGuiCol_TableHeaderBg] = ImVec4(0.1882352977991104f, 0.1882352977991104f, 0.2000000029802322f, 1.0f);
	style.Colors[ImGuiCol_TableBorderStrong] = ImVec4(0.3098039329051971f, 0.3098039329051971f, 0.3490196168422699f, 1.0f);
	style.Colors[ImGuiCol_TableBorderLight] = ImVec4(0.2274509817361832f, 0.2274509817361832f, 0.2470588237047195f, 1.0f);
	style.Colors[ImGuiCol_TableRowBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
	style.Colors[ImGuiCol_TableRowBgAlt] = ImVec4(1.0f, 1.0f, 1.0f, 0.05999999865889549f);
	style.Colors[ImGuiCol_TextSelectedBg] = ImVec4(0.8666666746139526f, 0.8666666746139526f, 0.8666666746139526f, 0.3499999940395355f);
	style.Colors[ImGuiCol_DragDropTarget] = ImVec4(1.0f, 1.0f, 0.0f, 0.8999999761581421f);
	style.Colors[ImGuiCol_NavHighlight] = ImVec4(0.6000000238418579f, 0.6000000238418579f, 0.6000000238418579f, 1.0f);
	style.Colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.0f, 1.0f, 1.0f, 0.699999988079071f);
	style.Colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.800000011920929f, 0.800000011920929f, 0.800000011920929f, 0.2000000029802322f);
	style.Colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.800000011920929f, 0.800000011920929f, 0.800000011920929f, 0.3499999940395355f);

    style.ScaleAllSizes(JSON_AS_TYPE(configMap["UIScaling"], float));
}