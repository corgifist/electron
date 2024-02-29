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
        void* userData;

        RendererInfo() {}
    };

    struct FramebufferInfo {
        GPUHandle fbo, stencil;
        int width, height;

        FramebufferInfo() {}
    };

    struct VAOInfo {
        GPUHandle buffer, vao;

        VAOInfo() {}
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
        static GPUHandle GeneratePipeline();
        static void BindPipeline(GPUHandle pipeline);
        static void UseProgramStages(ShaderType type, GPUHandle pipeline, GPUHandle program);
        static void MemoryBarrier(MemoryBarrierType barrier);
        static void DispatchCompute(int x, int y, int z);
        static GPUExtendedHandle GenerateVAO(std::vector<float>& vertices, std::vector<float>& uv);
        static void DestroyVAO(GPUExtendedHandle handle);
        static void BindVAO(GPUExtendedHandle handle);
        static void DrawArrays(int size);
        static GPUHandle GenerateShaderProgram(ShaderType type, const char* code);
        static void DestroyShaderProgram(GPUHandle program);
        static GPUHandle GetProgramUniformLocation(GPUHandle program, const char* uniform);
        static void ClearTextureImage(GPUHandle, int attachment, float* color);
        static GPUHandle GenerateGPUTexture(int width, int height);
        static GPUHandle ImportGPUTexture(uint8_t* texture, int width, int height, int channels);
        static GPUExtendedHandle GenerateFramebuffer(GPUHandle color, GPUHandle uv, int width, int height);
        static void BindFramebuffer(GPUExtendedHandle fbo);
        static void DestroyFramebuffer(GPUExtendedHandle fbo);
        static void DestroyGPUTexture(GPUHandle texture);
    };
};