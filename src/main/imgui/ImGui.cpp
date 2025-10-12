#include "ImGui.h"


#include <format>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_to_string.hpp>
#include "../backend/Swapchain.h"
#include "../backend/VulkanContext.h"
#include "../glfw/Window.h"
#include "../util/Logger.h"

ImGuiBackend::ImGuiBackend(const vk::Instance &instance, const vk::Device &device, const vk::PhysicalDevice &physical_device, const glfw::Window &window,
                           const Swapchain &swapchain, const DeviceQueue &queue, const vk::Format &depth_format) {
    IMGUI_CHECKVERSION();
    mContext = ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

#ifndef NDEBUG
    io.IniFilename = "local/imgui.ini";
    io.LogFilename = "local/imgui_log.txt";
#else
    // disable files in release mode
    io.IniFilename = nullptr;
    io.LogFilename = nullptr;
#endif

    ImGui_ImplGlfw_InitForVulkan(window, true);
    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.Instance = instance;
    init_info.PhysicalDevice = physical_device;
    init_info.Device = device;
    init_info.QueueFamily = queue.family;
    init_info.Queue = queue.queue;
    init_info.DescriptorPoolSize = IMGUI_IMPL_VULKAN_MINIMUM_IMAGE_SAMPLER_POOL_SIZE + 512;
    init_info.MinImageCount = swapchain.minImageCount();
    init_info.ImageCount = swapchain.imageCount();
    init_info.UseDynamicRendering = true;
    init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    init_info.CheckVkResultFn = [](VkResult result) {
        if (result != VK_SUCCESS) {
            Logger::fatal(std::format("ImGui Vulkan Error: {}", vk::to_string(static_cast<vk::Result>(result))));
        }
    };
    vk::Format color_attachment_format = swapchain.colorFormatLinear();
    init_info.PipelineRenderingCreateInfo = vk::PipelineRenderingCreateInfo{
        .colorAttachmentCount = 1,
        .pColorAttachmentFormats = &color_attachment_format,
        .depthAttachmentFormat = depth_format,
    };

    ImGui::StyleColorsDark();

    ImGui_ImplVulkan_Init(&init_info);
}

ImGuiBackend::ImGuiBackend() = default;

ImGuiBackend::~ImGuiBackend() {
    if (mContext) {
        ImGui_ImplVulkan_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
    }
}

void ImGuiBackend::beginFrame() const {
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void ImGuiBackend::render(const vk::CommandBuffer &cmd_buf) const {
    ImGui::Render();
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd_buf);
}
