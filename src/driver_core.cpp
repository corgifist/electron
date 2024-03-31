#include "driver_core.h"


#define VMA_DEBUG_LOG
#define VMA_IMPLEMENTATION
#include "utils/vk_mem_alloc.h"

#include "utils/glad/include/vulkan.h"

namespace Electron {

    RendererInfo DriverCore::renderer{};

    static std::set<GPUExtendedHandle> textureRegistry{};

    static bool IsTexture(GPUExtendedHandle ptr) {
        return textureRegistry.find(ptr) != textureRegistry.end();
    }

    void DriverCore::GLFWCallback(int id, const char *description) {
        print("DriverCore::GLFWCallback: " << id);
        print("DriverCore::GLFWCallback: " << description);
    }

    static VulkanFrameInfo& GetCurrentFrameInfo(VulkanRendererInfo* vk) {
        return vk->frames[Shared::frameID % MAX_FRAMES_IN_FLIGHT];
    }

    static VkImageSubresourceRange SubresourceRange(VkImageAspectFlags aspectMask) {
        VkImageSubresourceRange subresourceRange = {};
        subresourceRange.aspectMask = aspectMask;
        subresourceRange.baseMipLevel = 0;
        subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
        subresourceRange.baseArrayLayer = 0;
        subresourceRange.layerCount = 1;
        return subresourceRange;
    }

    static void TransitionVulkanTexture(VkCommandBuffer cmd, VkImage image, VulkanAllocatedTexture* texture, VkImageLayout currentLayout, VkImageLayout newLayout) {
        if (texture) {
            if (texture->imageLayout == newLayout) return;
            texture->imageLayout = newLayout;
        } 
        VkImageMemoryBarrier2 imageBarrier = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2
        };
        imageBarrier.pNext = nullptr;

        if (currentLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
            imageBarrier.srcAccessMask = 0;
            imageBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

            imageBarrier.srcStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            imageBarrier.dstStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
        } else if (currentLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
            imageBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            imageBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

            imageBarrier.srcStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
            imageBarrier.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        } else if (currentLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR) {
            imageBarrier.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            imageBarrier.dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;

            imageBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        } else if (currentLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) {
            imageBarrier.srcStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            imageBarrier.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

            imageBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        } else {
            imageBarrier.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
            imageBarrier.srcAccessMask = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT;
            imageBarrier.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
            imageBarrier.dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT;
        } 

        imageBarrier.oldLayout = currentLayout;
        imageBarrier.newLayout = newLayout;

        VkImageAspectFlagBits aspectMask = (newLayout == VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
        
        imageBarrier.subresourceRange = SubresourceRange(
            (newLayout == VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT
        );
        imageBarrier.image = image;

        VkDependencyInfo dependencyInfo = {};
        dependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
        dependencyInfo.pNext = nullptr;
        dependencyInfo.imageMemoryBarrierCount = 1;
        dependencyInfo.pImageMemoryBarriers = &imageBarrier;

        vkCmdPipelineBarrier2(cmd, &dependencyInfo);
    }

    static VkCommandBufferSubmitInfo CommandBufferSubmitInfo(VkCommandBuffer cmd) {
        VkCommandBufferSubmitInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
        info.pNext = nullptr;
        info.commandBuffer = cmd;
        info.deviceMask = 0;
        return info;
    }

    static VkSemaphoreSubmitInfo SemaphoreSubmitInfo(VkPipelineStageFlags2 stageMask, VkSemaphore semaphore) {
        VkSemaphoreSubmitInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
        info.pNext = nullptr;
        info.semaphore = semaphore;
        info.stageMask = stageMask;
        info.deviceIndex = 0;
        info.value = 1;
        return info;
    }

    static VkSubmitInfo2 SubmitInfo(VkCommandBufferSubmitInfo* cmd, VkSemaphoreSubmitInfo* signalSemaphoreInfo, VkSemaphoreSubmitInfo* waitSemaphoreInfo) {
        VkSubmitInfo2 info = {};
        info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
        info.pNext = nullptr;

        info.waitSemaphoreInfoCount = waitSemaphoreInfo == nullptr ? 0 : 1;
        info.pWaitSemaphoreInfos = waitSemaphoreInfo;

        info.signalSemaphoreInfoCount = signalSemaphoreInfo == nullptr ? 0 : 1;
        info.pSignalSemaphoreInfos = signalSemaphoreInfo;

        info.commandBufferInfoCount = 1;
        info.pCommandBufferInfos = cmd;
        return info;
    }

    static VkRenderingAttachmentInfo AttachmentInfo(VkImageView view, VkClearValue* clear, VkImageLayout layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) {
        VkRenderingAttachmentInfo colorAttachment = {};
        colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        colorAttachment.pNext = nullptr;

        colorAttachment.imageView = view;
        colorAttachment.imageLayout = layout;
        colorAttachment.loadOp = clear ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        if (clear) {
            colorAttachment.clearValue = *clear;
        }

        return colorAttachment;
    }

    static VkCommandBufferBeginInfo CommandBufferBeginInfo(VkCommandBufferUsageFlags flags) {
        VkCommandBufferBeginInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        info.pNext = nullptr;
        info.pInheritanceInfo = nullptr;
        info.flags = flags;
        return info;
    }

    static VkRenderingInfo RenderingInfo(VkExtent2D renderExtent, VkRenderingAttachmentInfo* colorAttachment, VkRenderingAttachmentInfo* depthAttachment) {
        VkRenderingInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
        info.pNext = nullptr;
        info.renderArea = VkRect2D{VkOffset2D{0, 0}, renderExtent};
        info.layerCount = 1;
        info.colorAttachmentCount = 1;
        info.pColorAttachments = colorAttachment;
        info.pDepthAttachment = depthAttachment;
        info.pStencilAttachment = nullptr;
        return info;
    }

    static void CopyImageToImage(VkCommandBuffer cmd, VkImage source, VkImage destination, VkExtent3D extent) {
       	VkImageBlit2 blitRegion{ .sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2, .pNext = nullptr };

        blitRegion.srcOffsets[1].x = extent.width;
        blitRegion.srcOffsets[1].y = extent.height;
        blitRegion.srcOffsets[1].z = 1;

        blitRegion.dstOffsets[1].x = extent.width;
        blitRegion.dstOffsets[1].y = extent.height;
        blitRegion.dstOffsets[1].z = 1;

        blitRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blitRegion.srcSubresource.baseArrayLayer = 0;
        blitRegion.srcSubresource.layerCount = 1;
        blitRegion.srcSubresource.mipLevel = 0;

        blitRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blitRegion.dstSubresource.baseArrayLayer = 0;
        blitRegion.dstSubresource.layerCount = 1;
        blitRegion.dstSubresource.mipLevel = 0;

        VkBlitImageInfo2 blitInfo{ .sType = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2, .pNext = nullptr };
        blitInfo.dstImage = destination;
        blitInfo.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        blitInfo.srcImage = source;
        blitInfo.srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        blitInfo.filter = VK_FILTER_LINEAR;
        blitInfo.regionCount = 1;
        blitInfo.pRegions = &blitRegion;

        vkCmdBlitImage2(cmd, &blitInfo);
    }

    static void CreateSyncObjects(VulkanRendererInfo* vk) {
        print("creating sync objects for " << MAX_FRAMES_IN_FLIGHT << " frames in flight");
        VkFenceCreateInfo fenceCreateInfo = {};
        fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceCreateInfo.pNext = nullptr;
        fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        VkSemaphoreCreateInfo semaphoreCreateInfo = {};
        semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        semaphoreCreateInfo.pNext = nullptr;
        semaphoreCreateInfo.flags = 0;

        for (auto& frame : vk->frames) {
            vkCreateFence(vk->device.device, &fenceCreateInfo, nullptr, &frame.renderFence);

            vkCreateSemaphore(vk->device.device, &semaphoreCreateInfo, nullptr, &frame.swapchainSemaphore);
            vkCreateSemaphore(vk->device.device, &semaphoreCreateInfo, nullptr, &frame.renderSemaphore);
        }
}

    static void DestroySyncObjects(VulkanRendererInfo* vk) {
        for (auto& frame : vk->frames) {
            vkDestroyFence(vk->device.device, frame.renderFence, nullptr);
            vkDestroySemaphore(vk->device.device, frame.swapchainSemaphore, nullptr);
            vkDestroySemaphore(vk->device.device, frame.renderSemaphore, nullptr);
        }
    }

    static VulkanAllocatedBuffer ExplicitAllocateBuffer(size_t size, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage, bool useHostCached = false) {
        print("allocating buffer with size " << size);
        VulkanRendererInfo* vk = (VulkanRendererInfo*) DriverCore::renderer.userData;

        VkBufferCreateInfo bufferCreateInfo = {};
        bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferCreateInfo.pNext = nullptr;
        bufferCreateInfo.size = size;
        bufferCreateInfo.usage = usage;

        VmaAllocationCreateInfo allocationCreateInfo = {};
        allocationCreateInfo.usage = memoryUsage;
        allocationCreateInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;
        if (useHostCached) {
            allocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;
            allocationCreateInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
        }

        VulkanAllocatedBuffer allocatedBuffer;
        vmaCreateBuffer(vk->allocator, &bufferCreateInfo, &allocationCreateInfo, &allocatedBuffer.buffer, &allocatedBuffer.bufferAllocation, &allocatedBuffer.allocationInfo);
        return allocatedBuffer;
    }

    static GPUHandle swapchainImageIndex = 0;
    static VulkanFrameInfo* frameInfo;
    static VkResult swapchainImageResult = VK_SUCCESS;

    static void ResizeSwapchain(int width, int height) {
        VulkanRendererInfo* vk = (VulkanRendererInfo*) DriverCore::renderer.userData;
        vkb::SwapchainBuilder swapchainBuilder{vk->device, vk->surface};
        print("resizing vulkan swapchain");

        auto tempSwapchain = swapchainBuilder
                    .set_old_swapchain(vk->swapchain)
                    .set_desired_format(
                        VkSurfaceFormatKHR{
                            .format = SWAPCHAIN_FORMAT,
                            .colorSpace = COLOR_SPACE_FORMAT
                        }
                    )
                    .set_desired_present_mode(VK_PRESENT_MODE_IMMEDIATE_KHR)
                    .set_desired_extent(width, height)
                    .add_image_usage_flags(VK_IMAGE_USAGE_TRANSFER_DST_BIT)
                    .build();
        for (auto& swapchainImageView : vk->swapchainImageViews) {
            vkDestroyImageView(vk->device.device, swapchainImageView, nullptr);
        }
        vkb::destroy_swapchain(vk->swapchain);
        vk->swapchain = tempSwapchain.value();
        vk->renderExtent = {(uint32_t) width, (uint32_t) height};
        vk->swapchainImages = vk->swapchain.get_images().value();
        vk->swapchainImageViews = vk->swapchain.get_image_views().value();
    }

    static VkPresentInfoKHR PresentInfo() {
        VulkanRendererInfo* vk = (VulkanRendererInfo*) DriverCore::renderer.userData;
        VkPresentInfoKHR presentInfo = {};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.pNext = nullptr;
        presentInfo.pSwapchains = &vk->swapchain.swapchain;
        presentInfo.swapchainCount = 1;

        presentInfo.pWaitSemaphores = &frameInfo->renderSemaphore;
        presentInfo.waitSemaphoreCount = 1;

        presentInfo.pImageIndices = &swapchainImageIndex;

        return presentInfo;
    }

    static VulkanAllocatedTexture* ExplicitAllocateImage(int width, int height, VkFormat format, VkImageUsageFlags usage, int mipLevels = 1) {
        print("creating image with resolution " << width << " " << height);
        VulkanRendererInfo* vk = (VulkanRendererInfo*) DriverCore::renderer.userData;
        VulkanAllocatedTexture* texture = new VulkanAllocatedTexture();
        texture->imageExtent = {
            (uint32_t) width, (uint32_t) height, 1
        };
        texture->imageFormat = format;
        texture->imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        
        VkImageUsageFlags drawImageUsages = usage;

        VkImageCreateInfo imageCreateInfo = {};
        imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageCreateInfo.pNext = nullptr;
        imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
        imageCreateInfo.format = texture->imageFormat;
        imageCreateInfo.extent = texture->imageExtent;
        imageCreateInfo.mipLevels = mipLevels;
        imageCreateInfo.arrayLayers = 1;
        imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageCreateInfo.usage = drawImageUsages;

        VmaAllocationCreateInfo imageAllocationInfo = {};
        imageAllocationInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
        imageAllocationInfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        vmaCreateImage(vk->allocator, &imageCreateInfo, &imageAllocationInfo, &texture->image, &texture->imageAllocation, nullptr);


        VkImageViewCreateInfo imageViewCreateInfo = {};
        imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        imageViewCreateInfo.pNext = nullptr;

        imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        imageViewCreateInfo.image = texture->image;
        imageViewCreateInfo.format = texture->imageFormat;
        imageViewCreateInfo.subresourceRange = SubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT);

        vkCreateImageView(vk->device.device, &imageViewCreateInfo, nullptr, &texture->imageView);

        VkSamplerCreateInfo samplerCreateInfo = {};
        samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerCreateInfo.pNext = nullptr;
        samplerCreateInfo.magFilter = VK_FILTER_LINEAR;
        samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
        samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        samplerCreateInfo.maxLod = mipLevels;

        vkCreateSampler(vk->device.device, &samplerCreateInfo, nullptr, &texture->imageSampler);

        textureRegistry.insert((GPUExtendedHandle) texture);

        return texture;
    }

    static void ExplicitDestroyBuffer(VulkanAllocatedBuffer buffer) {
       VulkanRendererInfo* vk = (VulkanRendererInfo*) DriverCore::renderer.userData;
       vmaDestroyBuffer(vk->allocator, buffer.buffer, buffer.bufferAllocation);
    }

    static std::vector<std::function<void()>> deferredExecutionQueue;

    static void PushDeferredFunction(std::function<void()> f) {
        deferredExecutionQueue.push_back(f);
    }

    // DRIVER CORE API

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
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        std::vector<int> maybeSize = {
            JSON_AS_TYPE(Shared::configMap["LastWindowSize"].at(0), int),
            JSON_AS_TYPE(Shared::configMap["LastWindowSize"].at(1), int)};

        GLFWwindow* displayHandle = glfwCreateWindow(
            maybeSize[0],
            maybeSize[1], "Electron", nullptr, nullptr);
        renderer.displayHandle = displayHandle;

        VulkanRendererInfo* vk = new VulkanRendererInfo();
        renderer.userData = vk;

        vk->renderExtent = {(uint32_t) maybeSize[0], (uint32_t) maybeSize[1]};

        vkb::InstanceBuilder instanceBuilder;
        vk->instance = instanceBuilder 
                            .set_app_name("Electron")
                            .set_engine_name("Electron Renderer")
                            .require_api_version(1, 3, 0)
                            .request_validation_layers()
                            .use_default_debug_messenger()
                            .build().value();
        vk->instanceDispatchTable = vk->instance.make_table();

        if (glfwCreateWindowSurface(vk->instance, displayHandle, nullptr, &vk->surface) != VK_SUCCESS) {
            throw std::runtime_error("cannot create vulkan surface");
        }

        VkPhysicalDeviceVulkan13Features features13 = {};
        features13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
        features13.dynamicRendering = true;
        features13.synchronization2 = true;

        VkPhysicalDeviceVulkan12Features features12 = {};
        features12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
        features12.bufferDeviceAddress = true;
        features12.descriptorIndexing = true;
        features12.runtimeDescriptorArray = true;
        features12.descriptorBindingPartiallyBound = true;
        features12.descriptorBindingVariableDescriptorCount = true;
        features12.shaderSampledImageArrayNonUniformIndexing = true;

        vkb::PhysicalDeviceSelector physicalDeviceSelector(vk->instance);
        vk->physicalDevice = physicalDeviceSelector
                                .set_surface(vk->surface)
                                .require_present(true)
                                .prefer_gpu_device_type(vkb::PreferredDeviceType::discrete)
                                .set_required_features_12(features12)
                                .set_required_features_13(features13)
                                .add_required_extension("VK_KHR_push_descriptor")
                                .select().value();

        if (!gladLoadVulkanUserPtr(vk->physicalDevice, (GLADuserptrloadfunc) glfwGetInstanceProcAddress, vk->instance.instance)) {
            throw std::runtime_error("cannot load vulkan function pointers! is vulkan supported on your machine?");
        }
        DUMP_VAR(vk->physicalDevice.name);

        frameInfo = &GetCurrentFrameInfo(vk);

        vkb::DeviceBuilder deviceBuilder(vk->physicalDevice);
        vk->device = deviceBuilder
            .build().value();

        vk->deviceDispatchTable = vk->device.make_table();
        vk->graphicsQueue = vk->device.get_queue(vkb::QueueType::graphics).value();
        vk->presentQueue = vk->device.get_queue(vkb::QueueType::present).value();
        vk->transferQueue = vk->device.get_queue(vkb::QueueType::transfer).value();

        vk->graphicsQueueFamily = vk->device.get_queue_index(vkb::QueueType::graphics).value();
        vk->presentQueueFamily = vk->device.get_queue_index(vkb::QueueType::present).value();
        vk->transferQueueFamily = vk->device.get_queue_index(vkb::QueueType::transfer).value();

        Shared::deviceName = vk->physicalDevice.name;

        ResizeSwapchain(maybeSize[0], maybeSize[1]);

        VmaAllocatorCreateInfo allocatorCreateInfo = {};
        allocatorCreateInfo.physicalDevice = vk->physicalDevice.physical_device;
        allocatorCreateInfo.device = vk->device.device;
        allocatorCreateInfo.instance = vk->instance.instance;
        allocatorCreateInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
        vmaCreateAllocator(&allocatorCreateInfo, &vk->allocator);

        VkCommandPoolCreateInfo commandPoolCreateInfo = {};
        commandPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        commandPoolCreateInfo.pNext = nullptr;
        commandPoolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        commandPoolCreateInfo.queueFamilyIndex = vk->graphicsQueueFamily;

        for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            vkCreateCommandPool(vk->device.device, &commandPoolCreateInfo, nullptr, &vk->frames[i].commandPool);

            VkCommandBufferAllocateInfo commandBufferAllocateInfo = {};
            commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
            commandBufferAllocateInfo.pNext = nullptr;
            commandBufferAllocateInfo.commandPool = vk->frames[i].commandPool;
            commandBufferAllocateInfo.commandBufferCount = 1;
            commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

            vkAllocateCommandBuffers(vk->device.device, &commandBufferAllocateInfo, &vk->frames[i].mainCommandBuffer);
        }

        CreateSyncObjects(vk);

        glfwSetFramebufferSizeCallback(displayHandle, [](GLFWwindow* displayHandle, int width, int height) {
            VulkanRendererInfo* vk = (VulkanRendererInfo*) renderer.userData;
            vkDeviceWaitIdle(vk->device.device);
            ResizeSwapchain(width, height);
        });

	    VkDescriptorPoolSize pool_sizes[] = {
            { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, DESCRIPTOR_POOL_SIZE }};

        VkDescriptorPoolCreateInfo poolCreateInfo = {};
        poolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolCreateInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        poolCreateInfo.maxSets = DESCRIPTOR_POOL_SIZE;
        poolCreateInfo.poolSizeCount = (uint32_t) std::size(pool_sizes);
        poolCreateInfo.pPoolSizes = pool_sizes;

        vkCreateDescriptorPool(vk->device.device, &poolCreateInfo, nullptr, &vk->imguiData.descriptorPool);

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO &io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.ConfigFlags |= ImGuiConfigFlags_DpiEnableScaleFonts;
        io.ConfigWindowsMoveFromTitleBarOnly = true;
        io.WantSaveIniSettings = true;
        io.IniFilename = "misc/imgui.ini";

        VkFormat formats[] = {
            SWAPCHAIN_FORMAT
        };

        VkPipelineRenderingCreateInfoKHR dynamicRenderingCreateInfo = {};
        dynamicRenderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
        dynamicRenderingCreateInfo.pNext = nullptr;
        dynamicRenderingCreateInfo.colorAttachmentCount = 1;
        dynamicRenderingCreateInfo.pColorAttachmentFormats = formats;

        ImGui_ImplVulkan_InitInfo init = {};
        init.Instance = vk->instance.instance;
        init.PhysicalDevice = vk->physicalDevice.physical_device;
        init.Device = vk->device.device;
        init.QueueFamily = vk->graphicsQueueFamily;
        init.Queue = vk->graphicsQueue;
        init.DescriptorPool = vk->imguiData.descriptorPool;
        init.MinImageCount = MAX_FRAMES_IN_FLIGHT;
        init.ImageCount = MAX_FRAMES_IN_FLIGHT;
        init.PipelineRenderingCreateInfo = dynamicRenderingCreateInfo;
        init.UseDynamicRendering = true;

        ImGui_ImplGlfw_InitForVulkan((GLFWwindow*) renderer.displayHandle, true);
        ImGui_ImplVulkan_Init(&init);

        DUMP_VAR(ImGui::GetIO().BackendRendererName);

        renderer.vendor = vk->physicalDevice.name;
        uint32_t instanceVersion;
        vkEnumerateInstanceVersion(&instanceVersion);

        uint32_t apiMajor = VK_API_VERSION_MAJOR(instanceVersion);
        uint32_t apiMinor = VK_API_VERSION_MINOR(instanceVersion);
        uint32_t apiPatch = VK_API_VERSION_PATCH(instanceVersion);
        renderer.version = string_format("%i.%i.%i", (int) apiMajor, (int) apiMinor, (int) apiPatch);
        renderer.renderer = "Vulkan";
    }

    static VkDescriptorType InterpretDescriptorType(DescriptorType type) {
        switch (type) {
            case DescriptorType::Sampler: {
                return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            }
            case DescriptorType::UniformBuffer: {
                return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            }
        }
        return (VkDescriptorType) 0;
    }

    static VkShaderStageFlagBits InterpretShaderType(ShaderType type) {
        switch (type) {
            case ShaderType::Vertex: {
                return VK_SHADER_STAGE_VERTEX_BIT;
            }
            case ShaderType::Fragment: {
                return VK_SHADER_STAGE_FRAGMENT_BIT;
            }
            case ShaderType::VertexFragment: {
                return VK_SHADER_STAGE_ALL;
            }
            case ShaderType::Compute: {
                return VK_SHADER_STAGE_COMPUTE_BIT;
            }
        }
        return (VkShaderStageFlagBits) 0;
    }

    static VkDescriptorBindingFlags InterpretDescriptorFlags(DescriptorFlags type) {
        switch (type) {
            case DescriptorFlags::Variable: {
                return VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT | VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT;
            }
        }
        return 0;
    }

    static VkPrimitiveTopology InterpretTriangleTopology(TriangleTopology topology) {
        switch (topology) {
            case TriangleTopology::Point: return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
            case TriangleTopology::Line: return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
            case TriangleTopology::Triangle: return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        }
        return (VkPrimitiveTopology) 0;
    } 

    static VkPolygonMode InterpretPolygonMode(PolygonMode polygonMode) {
        switch (polygonMode) {
            case PolygonMode::Fill: return VK_POLYGON_MODE_FILL;
            case PolygonMode::Line: return VK_POLYGON_MODE_LINE;
            case PolygonMode::Point: return VK_POLYGON_MODE_POINT;
        }
        return (VkPolygonMode) 0;
    }

    GPUExtendedHandle DescriptorSetLayoutBinding::Build() {
        VkDescriptorSetLayoutBinding* binding = new VkDescriptorSetLayoutBinding();
        binding->binding = bindingPoint;
        binding->descriptorType = InterpretDescriptorType(descriptorType);
        binding->descriptorCount = flag == DescriptorFlags::Variable ? 128u : 1;
        binding->pImmutableSamplers = nullptr;
        binding->stageFlags = InterpretShaderType(shaderType);
        return (GPUExtendedHandle) binding;
    }

    void DescriptorSetLayoutBuilder::AddBinding(DescriptorSetLayoutBinding binding) {
        bindings.push_back(binding);
    }

    GPUExtendedHandle DescriptorSetLayoutBuilder::Build() {
        VulkanRendererInfo* vk = (VulkanRendererInfo*) DriverCore::renderer.userData;
        std::vector<VkDescriptorSetLayoutBinding> builtBindings;
        std::vector<VkDescriptorSetLayoutBinding*> bindingPtrs;
        std::vector<VkDescriptorBindingFlags> bindingFlags;
        for (auto& binding : bindings) {
            auto builtPtr = (VkDescriptorSetLayoutBinding*) binding.Build();
            bindingFlags.push_back(InterpretDescriptorFlags(binding.flag));
            bindingPtrs.push_back(builtPtr);
            builtBindings.push_back(*builtPtr);
        }


        VkDescriptorSetLayoutBindingFlagsCreateInfo bindingFlagsCreateInfo = {};
        bindingFlagsCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
        bindingFlagsCreateInfo.bindingCount = bindingFlags.size();
        bindingFlagsCreateInfo.pBindingFlags = bindingFlags.data();

        VkDescriptorSetLayoutCreateInfo setLayoutCreateInfo = {};
        setLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        setLayoutCreateInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR;
        setLayoutCreateInfo.pNext = &bindingFlagsCreateInfo;
        setLayoutCreateInfo.bindingCount = builtBindings.size();
        setLayoutCreateInfo.pBindings = builtBindings.data();

        VkDescriptorSetLayout* layout = new VkDescriptorSetLayout();


        if (vkCreateDescriptorSetLayout(vk->device.device, &setLayoutCreateInfo, nullptr, layout) != VK_SUCCESS) {
            delete layout;
            return 0;
        }

        for (auto& ptr : bindingPtrs) {
            delete ptr;
        }

        return (GPUExtendedHandle) layout;
    }

    GPUExtendedHandle RenderPipelinePushConstantRange::Build() {
        VkPushConstantRange* pushConstantRange = new VkPushConstantRange();
        pushConstantRange->offset = offset;
        pushConstantRange->size = size;
        pushConstantRange->stageFlags = InterpretShaderType(shaderStage);
        return (GPUExtendedHandle) pushConstantRange;
    }

    void RenderPipelineLayoutBuilder::AddPushConstantRange(GPUExtendedHandle pushConstantRange) {
        this->pushConstantRanges.push_back(pushConstantRange);
    }
    void RenderPipelineLayoutBuilder::AddSetLayout(GPUExtendedHandle setLayout) {
        this->setLayouts.push_back(setLayout);
    }

    GPUExtendedHandle RenderPipelineLayoutBuilder::Build() {
        VulkanRendererInfo* vk = (VulkanRendererInfo*) DriverCore::renderer.userData;
        VkPipelineLayout* pipelineLayout = new VkPipelineLayout();

        std::vector<VkPushConstantRange> actualRanges;
        for (auto& pRange : pushConstantRanges) {
            actualRanges.push_back(*((VkPushConstantRange*) pRange));
        }

        std::vector<VkDescriptorSetLayout> actualSetLayouts;
        for (auto& pSetLayout : setLayouts) {
            actualSetLayouts.push_back(*((VkDescriptorSetLayout*) pSetLayout));
        }

        VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {};
        pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutCreateInfo.pNext = nullptr;
        pipelineLayoutCreateInfo.pushConstantRangeCount = pushConstantRanges.size();
        pipelineLayoutCreateInfo.pPushConstantRanges = pushConstantRanges.size() > 0 ? actualRanges.data() : nullptr;
        pipelineLayoutCreateInfo.setLayoutCount = setLayouts.size();
        pipelineLayoutCreateInfo.pSetLayouts = setLayouts.size() != 0 ? actualSetLayouts.data() : nullptr;

        if (vkCreatePipelineLayout(vk->device.device, &pipelineLayoutCreateInfo, nullptr, pipelineLayout) != VK_SUCCESS) {
            throw std::runtime_error("cannot create pipeline layout!");
        }

        for (auto& pRange : pushConstantRanges) {
            delete (VkPushConstantRange*) pRange;
        }
        for (auto& setLayout : setLayouts) {
            delete (VkDescriptorSetLayout*) setLayout;
        }

        return (GPUExtendedHandle) pipelineLayout;
    }

    GPUExtendedHandle RenderPipelineShaderStageBuilder::Build() {
        VkPipelineShaderStageCreateInfo* shaderStageCreateInfo = new VkPipelineShaderStageCreateInfo();

        VulkanAllocatedShader* shader = (VulkanAllocatedShader*) shaderModule;

        shaderStageCreateInfo->sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        shaderStageCreateInfo->pNext = nullptr;
        shaderStageCreateInfo->stage = InterpretShaderType(shaderStage);
        shaderStageCreateInfo->module = shader->shaderModule;
        shaderStageCreateInfo->pName = name.c_str();

        return (GPUExtendedHandle) shaderStageCreateInfo;
    }

    RenderPipelineBuilder::RenderPipelineBuilder() {
        this->triangleTopology = TriangleTopology::Triangle;
        this->polygonMode = PolygonMode::Fill;
        this->lineWidth = 1.0f;
    }

    GPUExtendedHandle RenderPipelineBuilder::Build() {
        VulkanRendererInfo* vk = (VulkanRendererInfo*) DriverCore::renderer.userData;
        std::vector<VkDynamicState> dynamicStates = {
            VK_DYNAMIC_STATE_VIEWPORT, 
            VK_DYNAMIC_STATE_SCISSOR
        };

        VkPipelineDynamicStateCreateInfo dynamicState = {};
        dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamicState.dynamicStateCount = dynamicStates.size();
        dynamicState.pDynamicStates = dynamicStates.data();

        VkPipelineVertexInputStateCreateInfo vertexInputInfo = {};
        vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        
        VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
        inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssembly.topology = InterpretTriangleTopology(triangleTopology);
        inputAssembly.primitiveRestartEnable = VK_FALSE;

        VkViewport viewport = {};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = vk->renderExtent.width;
        viewport.height = vk->renderExtent.height;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;

        VkRect2D scissor = {};
        scissor.extent = vk->renderExtent;

        VkPipelineViewportStateCreateInfo viewportState = {};
        viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportState.viewportCount = 1;
        viewportState.scissorCount = 1;
        viewportState.pViewports = &viewport;
        viewportState.pScissors = &scissor;

        VkPipelineRasterizationStateCreateInfo rasterizer = {};
        rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizer.depthClampEnable = VK_FALSE;
        rasterizer.rasterizerDiscardEnable = VK_FALSE;
        rasterizer.polygonMode = InterpretPolygonMode(polygonMode);
        rasterizer.lineWidth = lineWidth;
        rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
        rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
        rasterizer.depthBiasEnable = VK_FALSE;

        VkPipelineMultisampleStateCreateInfo multisampling = {};
        multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampling.sampleShadingEnable = VK_FALSE;
        multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
        colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        colorBlendAttachment.blendEnable = VK_FALSE;

        VkPipelineColorBlendAttachmentState colorBlendAttachments[2] = {
            colorBlendAttachment, colorBlendAttachment
        };

        VkPipelineColorBlendStateCreateInfo colorBlending = {};
        colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlending.logicOpEnable = VK_FALSE;
        colorBlending.attachmentCount = 2;
        colorBlending.pAttachments = colorBlendAttachments;
        
        static VkFormat framebufferFormats[] = {
            SWAPCHAIN_FORMAT, SWAPCHAIN_FORMAT
        };

        VkPipelineRenderingCreateInfo pipelineRenderingCreateInfo = {};
        pipelineRenderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
        pipelineRenderingCreateInfo.colorAttachmentCount = 2;
        pipelineRenderingCreateInfo.pColorAttachmentFormats = framebufferFormats;

        std::vector<VkPipelineShaderStageCreateInfo> tempStages;
        for (auto& stage : stages) {
            tempStages.push_back(*((VkPipelineShaderStageCreateInfo*) stage));
        }

        VkGraphicsPipelineCreateInfo pipelineInfo = {};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineInfo.pVertexInputState = &vertexInputInfo;
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pViewportState = &viewportState;
        pipelineInfo.pRasterizationState = &rasterizer;
        pipelineInfo.pMultisampleState = &multisampling;
        pipelineInfo.pDepthStencilState = nullptr;
        pipelineInfo.pColorBlendState = &colorBlending;
        pipelineInfo.pDynamicState = &dynamicState;
        pipelineInfo.pNext = &pipelineRenderingCreateInfo;
        pipelineInfo.stageCount = stages.size();
        pipelineInfo.pStages = tempStages.data();

        pipelineInfo.layout = *((VkPipelineLayout*) pipelineLayout);
        pipelineInfo.renderPass = nullptr;
        pipelineInfo.subpass = 0;
        
        VkPipeline* pipeline = new VkPipeline();
        if (vkCreateGraphicsPipelines(vk->device.device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, pipeline) != VK_SUCCESS) {
            throw std::runtime_error("cannot create graphics pipeline!");
        }
        return (GPUExtendedHandle) pipeline;
    }

    void DriverCore::BindPipeline(GPUExtendedHandle pipeline) {
        vkCmdBindPipeline(frameInfo->mainCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *((VkPipeline*) pipeline));
    }

    void DriverCore::PipelineBarrier() {
        VkMemoryBarrier2KHR memoryBarrier = {
            .srcStageMask = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
            .srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT_KHR,
            .dstStageMask = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT,
            .dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT_KHR
        };

        VkDependencyInfo dependencyInfo = {
            .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
            .memoryBarrierCount = 1,
            .pMemoryBarriers = &memoryBarrier
        };

        vkCmdPipelineBarrier2(frameInfo->mainCommandBuffer, &dependencyInfo);
    }

    GPUExtendedHandle DriverCore::GenerateGPUTexture(int width, int height, bool keepStagingBuffer) {
        VulkanAllocatedTexture* texture = ExplicitAllocateImage(
            width, height, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT
        );
        texture->hasStagingBuffer = keepStagingBuffer;
        if (keepStagingBuffer) {
            texture->stagingBuffer = ExplicitAllocateBuffer(width * height * 4, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU, true);
            TransitionVulkanTexture(frameInfo->mainCommandBuffer, texture->image, texture, texture->imageLayout, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        }
        return (GPUExtendedHandle) texture;
    }

    GPUExtendedHandle DriverCore::ImportGPUTexture(uint8_t* texture, int width, int height, int channels, bool keepStagingBuffer) {
        VulkanRendererInfo* vk = (VulkanRendererInfo*) renderer.userData;

        VulkanAllocatedTexture* allocatedTexture = ExplicitAllocateImage(width, height, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, MIPMAP_LEVELS);

        allocatedTexture->stagingBuffer = ExplicitAllocateBuffer(width * height * channels, VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
        allocatedTexture->hasStagingBuffer = keepStagingBuffer;
        memcpy(allocatedTexture->stagingBuffer.allocationInfo.pMappedData, texture, width * height * channels);

        VkCommandBuffer cmd = frameInfo->mainCommandBuffer;

        TransitionVulkanTexture(cmd, allocatedTexture->image, allocatedTexture, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
            
        VkBufferImageCopy copyRegion = {};
        copyRegion.bufferOffset = 0;
        copyRegion.bufferRowLength = 0;
        copyRegion.bufferImageHeight = 0;

        copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        copyRegion.imageSubresource.mipLevel = 0;
        copyRegion.imageSubresource.baseArrayLayer = 0;
        copyRegion.imageSubresource.layerCount = 1;
        copyRegion.imageExtent = {(uint32_t) width, (uint32_t) height, 1};

        vkCmdCopyBufferToImage(cmd, allocatedTexture->stagingBuffer.buffer, allocatedTexture->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

        if (!keepStagingBuffer)
            PushDeferredFunction([allocatedTexture]() {
                ExplicitDestroyBuffer(allocatedTexture->stagingBuffer);
            });

        VkImageMemoryBarrier mipmapBarrier = {};
        mipmapBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        mipmapBarrier.image = allocatedTexture->image;
        mipmapBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        mipmapBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        mipmapBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        mipmapBarrier.subresourceRange.baseArrayLayer = 0;
        mipmapBarrier.subresourceRange.layerCount = 1;
        mipmapBarrier.subresourceRange.levelCount = 1;


        int32_t mipWidth = width;
        int32_t mipHeight = height;

        TransitionVulkanTexture(frameInfo->mainCommandBuffer, allocatedTexture->image, allocatedTexture, allocatedTexture->imageLayout, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
        for (int i = 1; i < MIPMAP_LEVELS; i++) {
            VkImageBlit blit{};
            blit.srcOffsets[0] = { 0, 0, 0 };
            blit.srcOffsets[1] = { mipWidth, mipHeight, 1 };
            blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            blit.srcSubresource.mipLevel = i - 1;
            blit.srcSubresource.baseArrayLayer = 0;
            blit.srcSubresource.layerCount = 1;
            blit.dstOffsets[0] = { 0, 0, 0 };
            blit.dstOffsets[1] = { mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, 1 };
            blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            blit.dstSubresource.mipLevel = i;
            blit.dstSubresource.baseArrayLayer = 0;
            blit.dstSubresource.layerCount = 1;

            vkCmdBlitImage(frameInfo->mainCommandBuffer, allocatedTexture->image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, allocatedTexture->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_LINEAR);
            if (mipWidth > 1) mipWidth /= 2;
            if (mipHeight > 1) mipHeight /= 2;
        }

        TransitionVulkanTexture(frameInfo->mainCommandBuffer, allocatedTexture->image, allocatedTexture, allocatedTexture->imageLayout, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

        return (GPUExtendedHandle) allocatedTexture;
    }

    GPUExtendedHandle DriverCore::GenerateFramebuffer(GPUExtendedHandle color, GPUExtendedHandle uv, int width, int height) {
        VulkanAllocatedFramebuffer* fbo = new VulkanAllocatedFramebuffer();
        fbo->width = width;
        fbo->height = height;
        fbo->colorAttachment = color;
        fbo->uvAttachment = uv;
        return (GPUExtendedHandle) fbo;
    }

    void DriverCore::DispatchCompute(int x, int y, int z) {
    }

    void DriverCore::ClearTextureImage(GPUExtendedHandle ptr, int attachment, float* color) {
        if (!IsTexture(ptr)) return;
        VulkanRendererInfo* vk = (VulkanRendererInfo*) renderer.userData;
        VulkanAllocatedTexture* texture = (VulkanAllocatedTexture*) ptr;
        auto subresourceRange = SubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT);
        VkClearColorValue clearValue = {
            color[0], color[1], color[2], color[3]
        };
        
        VkImageLayout previousLayout = texture->imageLayout;
        TransitionVulkanTexture(frameInfo->mainCommandBuffer, texture->image, texture, previousLayout, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
            vkCmdClearColorImage(frameInfo->mainCommandBuffer, texture->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearValue, 1, &subresourceRange);
        if (previousLayout == VK_IMAGE_LAYOUT_UNDEFINED)
            previousLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        TransitionVulkanTexture(frameInfo->mainCommandBuffer, texture->image, texture, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, previousLayout);
    }

    static std::set<GPUExtendedHandle> imageHandles;

    GPUExtendedHandle DriverCore::GetImageHandleUI(GPUExtendedHandle ptr) {
        if (!ptr) return 0;
        if (!IsTexture(ptr)) return 0;
        VulkanAllocatedTexture* texture = (VulkanAllocatedTexture*) ptr;
        if (textureRegistry.find(ptr) == textureRegistry.end()) {
            textureRegistry.insert(ptr);
        }
        ImTextureID handle = ImGui_ImplVulkan_AddTexture(texture->imageSampler, texture->imageView, texture->imageLayout);
        imageHandles.insert((GPUExtendedHandle) handle);

        return (GPUExtendedHandle) handle;
    }

    void DriverCore::DestroyImageHandleUI(GPUExtendedHandle handle) {
        if (!handle) return;
        if (imageHandles.find(handle) == imageHandles.end()) return;
        PushDeferredFunction([handle]() {
            imageHandles.erase(handle);
            ImGui_ImplVulkan_RemoveTexture((VkDescriptorSet) handle);
        });
    }

    void DriverCore::PushDescriptors(std::vector<DescriptorWriteBinding> bindings, GPUExtendedHandle pipelineLayout, uint32_t set) {
        if (bindings.size() == 0) return;
        VkPipelineLayout layout = *((VkPipelineLayout*) pipelineLayout);

        std::vector<VkWriteDescriptorSet> writeSets;
        std::vector<VkDescriptorImageInfo*> destructionTargets;
        for (auto& binding : bindings) {
            VkWriteDescriptorSet writeSet = {};
            writeSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writeSet.dstSet = 0;
            writeSet.dstBinding = binding.binding;
            writeSet.descriptorCount = 1;
            writeSet.descriptorType = InterpretDescriptorType(binding.type);

            if (binding.type == DescriptorType::Sampler && IsTexture(binding.texture)) {
                VulkanAllocatedTexture* texture = ((VulkanAllocatedTexture*) binding.texture);
                VkDescriptorImageInfo* writeImageInfo = new VkDescriptorImageInfo();
                writeImageInfo->imageLayout = texture->imageLayout;
                writeImageInfo->imageView = texture->imageView;
                writeImageInfo->sampler = texture->imageSampler;
                
                writeSet.pImageInfo = writeImageInfo;

                destructionTargets.push_back(writeImageInfo);
            }

            writeSets.push_back(writeSet);
        }

        vkCmdPushDescriptorSetKHR(frameInfo->mainCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, set, writeSets.size(), writeSets.data());
        for (auto& target : destructionTargets) {
            delete target;
        }
    }

    void DriverCore::BeginRendering(GPUExtendedHandle ptr) {
        VulkanAllocatedFramebuffer* fbo = (VulkanAllocatedFramebuffer*) ptr;
        VulkanAllocatedTexture* colorTexture = (VulkanAllocatedTexture*) fbo->colorAttachment;
        VulkanAllocatedTexture* uvTexture = (VulkanAllocatedTexture*) fbo->uvAttachment;
        VkRenderingAttachmentInfo colorAttachment = AttachmentInfo(colorTexture->imageView, nullptr);
        VkRenderingAttachmentInfo uvAttachment = AttachmentInfo(uvTexture->imageView, nullptr);

        VkRenderingAttachmentInfo attachments[2] = {
            colorAttachment, uvAttachment
        };

        VkRenderingInfo renderingInfo = {};
        renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
        renderingInfo.colorAttachmentCount = 2;
        renderingInfo.pColorAttachments = attachments;
        renderingInfo.layerCount = 1;
        renderingInfo.renderArea.extent = {(uint32_t) fbo->width, (uint32_t) fbo->height};
    
        vkCmdBeginRendering(frameInfo->mainCommandBuffer, &renderingInfo);
    }

    void DriverCore::SetRenderingViewport(int width, int height) {
        
        VkViewport viewport = {};
        viewport.x = viewport.y = 0;
        viewport.maxDepth = 1.0f;
        viewport.minDepth = 0.0;
        viewport.width = width;
        viewport.height = height;

        VkRect2D scissor = {};
        scissor.extent = {(uint32_t) width, (uint32_t) height};

        vkCmdSetViewport(frameInfo->mainCommandBuffer, 0, 1, &viewport);
        vkCmdSetScissor(frameInfo->mainCommandBuffer, 0, 1, &scissor);
    }

    void DriverCore::DrawArrays(int size) {
        vkCmdDraw(frameInfo->mainCommandBuffer, size, 1, 0, 0);
    }

    void DriverCore::EndRendering() {        
        VulkanRendererInfo* vk = (VulkanRendererInfo*) renderer.userData;
        vkCmdEndRendering(frameInfo->mainCommandBuffer);
    }

    void DriverCore::PushConstants(GPUExtendedHandle layout, ShaderType shaderStage, size_t offset, size_t size, void* data) {
        VkPipelineLayout* pLayout = (VkPipelineLayout*) layout;
        vkCmdPushConstants(frameInfo->mainCommandBuffer, *pLayout, InterpretShaderType(shaderStage), offset, size, data);
    }

    GPUExtendedHandle DriverCore::GenerateShaderProgram(ShaderType shaderType, std::vector<uint8_t> binary) {
        VulkanAllocatedShader* shader = new VulkanAllocatedShader();
        VulkanRendererInfo* vk = (VulkanRendererInfo*) renderer.userData;

        VkShaderModuleCreateInfo shaderCreateInfo = {};
        shaderCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        shaderCreateInfo.pNext = nullptr;
        shaderCreateInfo.codeSize = binary.size();
        shaderCreateInfo.pCode = (uint32_t*) binary.data();

        shader->shaderType = shaderType;
        if (vkCreateShaderModule(vk->device.device, &shaderCreateInfo, nullptr, &shader->shaderModule) != VK_SUCCESS) {
            return (GPUExtendedHandle) nullptr;
        }
        return (GPUExtendedHandle) shader;
    }

    GPUHandle DriverCore::GetProgramUniformLocation(GPUHandle program, const char* uniform) {
        return (GPUHandle) 0;
    }

    void DriverCore::DestroyShaderProgram(GPUExtendedHandle program) {
        if (!program) return;
        VulkanRendererInfo* vk = (VulkanRendererInfo*) renderer.userData;
        VulkanAllocatedShader* shader = (VulkanAllocatedShader*) program;
        vkDestroyShaderModule(vk->device.device, shader->shaderModule, nullptr);
        delete shader;
    }

    void DriverCore::MemoryBarrier(MemoryBarrierType type) {
    }

    void DriverCore::DestroyFramebuffer(GPUExtendedHandle fbo) {
        delete ((VulkanAllocatedFramebuffer*) fbo);
    }

    void DriverCore::UpdateTextureStagingBuffer(GPUExtendedHandle ptr, int width, int height, uint8_t* data) {
        VulkanRendererInfo* vk = (VulkanRendererInfo*) renderer.userData;
        if (!ptr) return;
        VulkanAllocatedTexture* texture =(VulkanAllocatedTexture*) ptr;
        if (!texture->hasStagingBuffer) return;
        if (data) {
            vmaCopyMemoryToAllocation(vk->allocator, data, texture->stagingBuffer.bufferAllocation, 0, width * height * 4);
        }
    }

    void DriverCore::UpdateTextureData(GPUExtendedHandle ptr, int width, int height, uint8_t* data) {
        VulkanRendererInfo* vk = (VulkanRendererInfo*) renderer.userData;
        if (!ptr) return;
        VulkanAllocatedTexture* texture =(VulkanAllocatedTexture*) ptr;
        if (!texture->hasStagingBuffer) return;
        if (data) {
            vmaCopyMemoryToAllocation(vk->allocator, data, texture->stagingBuffer.bufferAllocation, 0, width * height * 4);
        }
        VkBufferImageCopy copyRegion = {};
        copyRegion.bufferOffset = 0;
        copyRegion.bufferRowLength = 0;
        copyRegion.bufferImageHeight = 0;

        copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        copyRegion.imageSubresource.mipLevel = 0;
        copyRegion.imageSubresource.baseArrayLayer = 0;
        copyRegion.imageSubresource.layerCount = 1;
        copyRegion.imageExtent = {(uint32_t) width, (uint32_t) height, 1};

        vkCmdCopyBufferToImage(frameInfo->mainCommandBuffer, texture->stagingBuffer.buffer, texture->image, texture->imageLayout, 1, &copyRegion);
    }

    void DriverCore::OptimizeTextureForRendering(GPUExtendedHandle texture) {
        if (!texture) return;
        if (!IsTexture(texture)) return;
        VulkanAllocatedTexture* pTexture = (VulkanAllocatedTexture*) texture;
        TransitionVulkanTexture(frameInfo->mainCommandBuffer, pTexture->image, pTexture, pTexture->imageLayout, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    }

    void DriverCore::DestroyGPUTexture(GPUExtendedHandle ptr) {
        if (!ptr) return;
        if (!IsTexture(ptr)) return;
        VulkanAllocatedTexture texture = *((VulkanAllocatedTexture*) ptr);
        VulkanRendererInfo* vk = (VulkanRendererInfo*) renderer.userData;
        textureRegistry.erase(ptr);
        PushDeferredFunction([vk, texture]() {
            vkDestroyImageView(vk->device.device, texture.imageView, nullptr);
            vmaDestroyImage(vk->allocator, texture.image, texture.imageAllocation);        
        });
        if (texture.hasStagingBuffer) {
            PushDeferredFunction([texture]() {
                ExplicitDestroyBuffer(texture.stagingBuffer);
            });
        }
        delete (VulkanAllocatedTexture*) ptr;
    }

    bool DriverCore::ShouldClose() {
        return glfwWindowShouldClose((GLFWwindow*) renderer.displayHandle);
    }

    void DriverCore::SwapBuffers() {
        VulkanRendererInfo* vk = (VulkanRendererInfo*) renderer.userData;

        VkCommandBufferSubmitInfo cmdInfo = CommandBufferSubmitInfo(frameInfo->mainCommandBuffer);
        VkSemaphoreSubmitInfo waitInfo = SemaphoreSubmitInfo(VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, frameInfo->swapchainSemaphore);
        VkSemaphoreSubmitInfo signalInfo = SemaphoreSubmitInfo(VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, frameInfo->renderSemaphore);


        VkSubmitInfo2 submitInfo = SubmitInfo(&cmdInfo, &signalInfo, &waitInfo);
        vkQueueSubmit2(vk->graphicsQueue, 1, &submitInfo, frameInfo->renderFence);

        VkPresentInfoKHR presentInfo = PresentInfo();

        vkQueuePresentKHR(vk->graphicsQueue, &presentInfo);

        Shared::frameID++;
        frameInfo = &GetCurrentFrameInfo(vk);
    }

    void DriverCore::ImGuiNewFrame() {
        glfwPollEvents();
        VulkanRendererInfo* vk = (VulkanRendererInfo*) renderer.userData;
        vkWaitForFences(vk->device.device, 1, &frameInfo->renderFence, true, UINT64_MAX);

        swapchainImageResult = vkAcquireNextImageKHR(vk->device.device, vk->swapchain.swapchain, UINT64_MAX, frameInfo->swapchainSemaphore, nullptr, &swapchainImageIndex);
        if (swapchainImageIndex == VK_ERROR_OUT_OF_DATE_KHR) {
            int width, height;
            GetDisplaySize(&width, &height);
            vkDeviceWaitIdle(vk->device.device);
            ResizeSwapchain(width, height);
        }
        vkResetFences(vk->device.device, 1, &frameInfo->renderFence);

        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        vkResetCommandBuffer(frameInfo->mainCommandBuffer, 0);

        VkCommandBufferBeginInfo cmdBeginInfo = CommandBufferBeginInfo(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
        vkBeginCommandBuffer(frameInfo->mainCommandBuffer, &cmdBeginInfo);
        for (auto& f : deferredExecutionQueue) {
            f();
        }
        deferredExecutionQueue.clear();
    }

    void DriverCore::ImGuiRender() {
        ImGui::Render();
        VulkanRendererInfo* vk = (VulkanRendererInfo*) renderer.userData;
        auto& swapchainImages = vk->swapchainImages;
        auto& swapchainImageViews = vk->swapchainImageViews;

        TransitionVulkanTexture(frameInfo->mainCommandBuffer, swapchainImages[swapchainImageIndex], nullptr, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        VkClearColorValue clearValue = {0.0f, 0.0f, 0.0f, 1.0f};
        VkImageSubresourceRange subresourceRange = SubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT);
        vkCmdClearColorImage(frameInfo->mainCommandBuffer, swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearValue, 1, &subresourceRange);

        TransitionVulkanTexture(frameInfo->mainCommandBuffer, swapchainImages[swapchainImageIndex], nullptr, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        VkRenderingAttachmentInfo colorAttachment = AttachmentInfo(swapchainImageViews[swapchainImageIndex], nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        VkRenderingInfo renderInfo = RenderingInfo(vk->renderExtent, &colorAttachment, nullptr);

        vkCmdBeginRendering(frameInfo->mainCommandBuffer, &renderInfo);
            ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), frameInfo->mainCommandBuffer);
        vkCmdEndRendering(frameInfo->mainCommandBuffer);
        TransitionVulkanTexture(frameInfo->mainCommandBuffer, swapchainImages[swapchainImageIndex], nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
    

        vkEndCommandBuffer(frameInfo->mainCommandBuffer);
    }

    void DriverCore::ImGuiShutdown() {
        VulkanRendererInfo* vk = (VulkanRendererInfo*) renderer.userData;
        vkDeviceWaitIdle(vk->device.device);
        for (auto& f : deferredExecutionQueue) {
            f();
        }
        deferredExecutionQueue.clear();
        for (auto& imageHandle : imageHandles) {
            ImGui_ImplVulkan_RemoveTexture((VkDescriptorSet) imageHandle);
        }
        ImGui_ImplVulkan_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();

        for (auto& frame : vk->frames) {
            vkDestroyCommandPool(vk->device.device, frame.commandPool, nullptr);
        }

        DestroySyncObjects(vk);

        vkDestroyDescriptorPool(vk->device.device, vk->imguiData.descriptorPool, nullptr);


        vkb::destroy_swapchain(vk->swapchain);
        vkb::destroy_surface(vk->instance, vk->surface);
        vkb::destroy_device(vk->device);
        vkb::destroy_instance(vk->instance);

        delete vk;

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

    int DriverCore::FramesInFlightCount() {
        return MAX_FRAMES_IN_FLIGHT;
    }

    int DriverCore::GetFrameInFlightIndex() {
        return swapchainImageIndex;
    }
}