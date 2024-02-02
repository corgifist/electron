#pragma once

#include "electron.h"
#include "shared.h"
#include "ImGui/imgui_impl_opengl3.h"
#include "ImGui/imgui_impl_glfw.h"
#include "ImGui/imgui.h"

namespace Electron {

    struct RendererInfo {
        std::string vendor, version, renderer;
        void* displayHandle;

        RendererInfo() {}
    };

    struct DriverCore {
        static RendererInfo renderer;

        static void GLFWCallback(int id, const char *description);
        static void Bootstrap();
        static bool ShouldClose();
        static void SwapBuffers();
        static void ImGuiNewFrame();
        static void ImGuiRender();
        static void ImGuiShutdown();
        static void SetTitle(std::string title);
        static void GetDisplayPos(int* x, int* y);
        static void GetDisplaySize(int* w, int* h);
        static double GetTime();
    };
};