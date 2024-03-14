#pragma once

#include "electron.h"
#include "shared.h"
#include "ImGui/imgui_impl_vulkan.h"
#include "ImGui/imgui_impl_glfw.h"
#include "ImGui/imgui.h"
#include "VkBootstrap/VkBootstrap.h"
#define VMA_DEBUG_LOG
#include "utils/vk_mem_alloc.h"

#define MAX_FRAMES_IN_FLIGHT 3
#define SWAPCHAIN_FORMAT VK_FORMAT_B8G8R8A8_UNORM
#define COLOR_SPACE_FORMAT VK_COLOR_SPACE_SRGB_NONLINEAR_KHR
#define DESCRIPTOR_POOL_SIZE 128

namespace Electron {

    struct RendererInfo {
        std::string vendor, version, renderer;
        void* displayHandle;
        void* userData;

        RendererInfo() {}
    };

    struct VulkanFrameInfo {
        VkCommandPool commandPool;
        VkCommandBuffer mainCommandBuffer;

        VkSemaphore swapchainSemaphore, renderSemaphore;
        VkFence renderFence;

        VulkanFrameInfo() {}
    };

    struct VulkanAllocatedTexture {
        VkImage image;
        VkImageView imageView;
        VmaAllocation imageAllocation;
        VkExtent3D imageExtent;
        VkFormat imageFormat;
        VkSampler imageSampler;
        VkImageLayout imageLayout;

        VulkanAllocatedTexture() {}
    };

    struct VulkanAllocatedBuffer {
        VkBuffer buffer;
        VmaAllocation bufferAllocation;
        VmaAllocationInfo allocationInfo;

        VulkanAllocatedBuffer() {}
    };

    struct VulkanAllocatedFramebuffer {
        GPUExtendedHandle colorAttachment;
        GPUExtendedHandle uvAttachment;
        int width, height;


        VulkanAllocatedFramebuffer() {}
    };

    struct VulkanRendererInfo {
        vkb::Instance instance;

        vkb::InstanceDispatchTable instanceDispatchTable;
        vkb::DispatchTable deviceDispatchTable;

        vkb::PhysicalDevice physicalDevice;
        vkb::Device device;

        VkQueue graphicsQueue, presentQueue, transferQueue;
        GPUHandle graphicsQueueFamily, presentQueueFamily, transferQueueFamily;

        VkSurfaceKHR surface;

        vkb::Swapchain swapchain;
        std::vector<VkImage> swapchainImages;
        std::vector<VkImageView> swapchainImageViews;

        VulkanFrameInfo frames[MAX_FRAMES_IN_FLIGHT];

        VmaAllocator allocator;

        VkDescriptorPool descriptorPool;

        VkExtent2D renderExtent;

        VkFence immediateFence;
        VkCommandBuffer immediateCommandBuffer;
        VkCommandPool immediateCommandPool; 

        VulkanRendererInfo() {}
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
        static void UpdateTextureData(GPUHandle texture, int width, int height, uint8_t* data);
        static GPUHandle GenerateShaderProgram(ShaderType type, const char* code);
        static void DestroyShaderProgram(GPUHandle program);
        static GPUHandle GetProgramUniformLocation(GPUHandle program, const char* uniform);
        static void ClearTextureImage(GPUExtendedHandle texture, int attachment, float* color);
        static GPUExtendedHandle GenerateGPUTexture(int width, int height);
        static GPUExtendedHandle ImportGPUTexture(uint8_t* texture, int width, int height, int channels);
        static GPUExtendedHandle GenerateFramebuffer(GPUExtendedHandle color, GPUExtendedHandle uv, int width, int height);
        static GPUExtendedHandle GetImageHandleUI(GPUExtendedHandle texture);
        static void DestroyImageHandleUI(GPUExtendedHandle imageHandle);
        static void BindFramebuffer(GPUExtendedHandle fbo);
        static void DestroyFramebuffer(GPUExtendedHandle fbo);
        static void DestroyGPUTexture(GPUExtendedHandle texture);
    };
};