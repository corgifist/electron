#pragma once

#include "electron.h"
#include "shared.h"
#include "ImGui/imgui_impl_vulkan.h"
#include "ImGui/imgui_impl_glfw.h"
#include "ImGui/imgui.h"
#include "utils/glad/include/vulkan.h"
#include "GLFW/glfw3.h"
#include "VkBootstrap/VkBootstrap.h"
#define VMA_DEBUG_LOG
#include "utils/vk_mem_alloc.h"
#include <span>

#define MAX_FRAMES_IN_FLIGHT 2
#define SWAPCHAIN_FORMAT VK_FORMAT_B8G8R8A8_UNORM
#define COLOR_SPACE_FORMAT VK_COLOR_SPACE_SRGB_NONLINEAR_KHR
#define DESCRIPTOR_POOL_SIZE 256
#define MIPMAP_LEVELS 1
#define QUEUES_COUNT 8

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

        std::vector<std::function<void()>> deferredExecutionQueue;

        VulkanFrameInfo() {}
    };

    struct VulkanAllocatedBuffer {
        VkBuffer buffer;
        VmaAllocation bufferAllocation;
        VmaAllocationInfo allocationInfo;

        VulkanAllocatedBuffer() {}
    };

    struct VulkanAllocatedTexture {
        VkImage image;
        VkImageView imageView;
        VmaAllocation imageAllocation;
        VkExtent3D imageExtent;
        VkFormat imageFormat;
        VkSampler imageSampler;
        VkImageLayout imageLayout;
        VulkanAllocatedBuffer stagingBuffer;
        bool hasStagingBuffer;

        VulkanAllocatedTexture() {}
    };

    struct VulkanAllocatedFramebuffer {
        GPUExtendedHandle colorAttachment;
        GPUExtendedHandle uvAttachment;
        int width, height;


        VulkanAllocatedFramebuffer() {}
    };

    struct VulkanAllocatedShader {
        VkShaderModule shaderModule;
        ShaderType shaderType;

        VulkanAllocatedShader() {}
    };


    struct VulkanRendererInfo {
        vkb::Instance instance;

        vkb::InstanceDispatchTable instanceDispatchTable;
        vkb::DispatchTable deviceDispatchTable;

        vkb::PhysicalDevice physicalDevice;
        vkb::Device device;

        VkQueue graphicsQueue, presentQueue, transferQueue;
        std::vector<VkQueue> additionalGraphicsQueues;
        GPUHandle graphicsQueueFamily, presentQueueFamily, transferQueueFamily;

        VkSurfaceKHR surface;

        vkb::Swapchain swapchain;
        std::vector<VkImage> swapchainImages;
        std::vector<VkImageView> swapchainImageViews;

        VulkanFrameInfo frames[MAX_FRAMES_IN_FLIGHT];

        VmaAllocator allocator;

        struct ImGuiVulkanData {
            VkDescriptorPool descriptorPool;
        } imguiData;

        VkExtent2D renderExtent;

        VulkanRendererInfo() {}
    };  
    
    enum class DescriptorType {
        UniformBuffer, Sampler, ShaderStorageBuffer
    };


    enum class TriangleTopology {
        Point, Line, Triangle
    };

    enum class PolygonMode {
        Fill, Line, Point
    };

    struct DescriptorSetLayoutBinding {
        int bindingPoint;
        DescriptorType descriptorType;
        ShaderType shaderType;

        DescriptorSetLayoutBinding() {}

        GPUExtendedHandle Build();
    };

    struct DescriptorSetLayoutBuilder {
        std::vector<DescriptorSetLayoutBinding> bindings;

        DescriptorSetLayoutBuilder() {}

        void AddBinding(DescriptorSetLayoutBinding binding);
        GPUExtendedHandle Build();
    };

    struct RenderPipelinePushConstantRange {
        size_t size, offset;
        ShaderType shaderStage;

        RenderPipelinePushConstantRange() {}

        GPUExtendedHandle Build();
    };

    struct RenderPipelineLayoutBuilder {
        std::vector<GPUExtendedHandle> pushConstantRanges;
        std::vector<GPUExtendedHandle> setLayouts;

        void AddPushConstantRange(GPUExtendedHandle pushConstantRange);
        void AddSetLayout(GPUExtendedHandle setLayout);

        GPUExtendedHandle Build();
    };

    struct RenderPipelineShaderStageBuilder {
        ShaderType shaderStage;
        GPUExtendedHandle shaderModule;
        std::string name;

        RenderPipelineShaderStageBuilder() {}

        GPUExtendedHandle Build();
    };

    struct RenderPipelineBuilder {
        std::vector<GPUExtendedHandle> stages;
        GPUExtendedHandle pipelineLayout;
        TriangleTopology triangleTopology;
        PolygonMode polygonMode;
        float lineWidth;

        RenderPipelineBuilder();

        GPUExtendedHandle Build();
    };

    struct DescriptorWriteBinding {
        DescriptorType type;
        uint32_t binding;
        GPUExtendedHandle resource;

        DescriptorWriteBinding() {}
    };

    struct VulkanAllocatedUniformBuffer {
        std::vector<VulkanAllocatedBuffer> buffers;
        std::vector<VulkanAllocatedBuffer> stagingBuffers;
        size_t size;

        VulkanAllocatedUniformBuffer() {}
    };
    
    struct VulkanAllocatedContext {
        VkCommandPool commandPool;
        VkCommandBuffer mainCommandBuffer;

        VkFence contextFence;

        VkQueue graphicsQueue;
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
        static void BindPipeline(GPUExtendedHandle context, GPUExtendedHandle pipeline);
        static void MemoryBarrier(MemoryBarrierType barrier);
        static void DispatchCompute(int x, int y, int z);
        
        static GPUExtendedHandle GenerateContext();
        static void WaitContextFence(GPUExtendedHandle context);
        static void BeginContext(GPUExtendedHandle context);
        static void EndContext(GPUExtendedHandle context);
        static void DestroyContext(GPUExtendedHandle context);

        static void PushDescriptors(GPUExtendedHandle context, std::vector<DescriptorWriteBinding> bindings, GPUExtendedHandle layout, uint32_t set);
        
        static void PipelineBarrier(GPUExtendedHandle context);
        static void BeginRendering(GPUExtendedHandle context, GPUExtendedHandle fbo);
        static void PushConstants(GPUExtendedHandle context, GPUExtendedHandle layout, ShaderType shaderStage, size_t offset, size_t size, void* data);
        static void SetRenderingViewport(GPUExtendedHandle context, int width, int height);
        static void DrawArrays(GPUExtendedHandle context, int size);
        static void EndRendering(GPUExtendedHandle context);

        static GPUExtendedHandle GenerateUniformBuffers(size_t size);
        static void UpdateUniformBuffers(GPUExtendedHandle context, GPUExtendedHandle ubo, void* data);
        static void DestroyUniformBuffers(GPUExtendedHandle ubo);

        static void UpdateTextureStagingBuffer(GPUExtendedHandle context, GPUExtendedHandle, int width, int height, uint8_t* data);

        static void UpdateTextureData(GPUExtendedHandle context, GPUExtendedHandle texture, int width, int height, uint8_t* data);
        
        static void* MapTransferBuffer(GPUExtendedHandle texture);

        static void OptimizeTextureForRendering(GPUExtendedHandle context, GPUExtendedHandle texture);

        static GPUExtendedHandle GenerateShaderProgram(ShaderType type, std::vector<uint8_t> binary);
        static void DestroyShaderProgram(GPUExtendedHandle program);
        static GPUHandle GetProgramUniformLocation(GPUHandle program, const char* uniform);
        
        static void ClearTextureImage(GPUExtendedHandle context, GPUExtendedHandle texture, int attachment, float* color);
        
        static GPUExtendedHandle GenerateGPUTexture(GPUExtendedHandle context, int width, int height, bool keepStagingBuffer = false);
        static GPUExtendedHandle ImportGPUTexture(GPUExtendedHandle context, uint8_t* texture, int width, int height, int channels, bool keepStagingBuffer = false);
        static void DestroyGPUTexture(GPUExtendedHandle texture);

        static GPUExtendedHandle GenerateFramebuffer(GPUExtendedHandle color, GPUExtendedHandle uv, int width, int height);
        static void DestroyFramebuffer(GPUExtendedHandle fbo);

        static GPUExtendedHandle GetImageHandleUI(GPUExtendedHandle texture);
        static void DestroyImageHandleUI(GPUExtendedHandle imageHandle);

        static int FramesInFlightCount();
        static int GetFrameInFlightIndex();
    };
};