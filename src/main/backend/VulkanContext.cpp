#define VMA_IMPLEMENTATION

#include "VulkanContext.h"

#include <iostream>
#include <VkBootstrap.h>
#include <glfw/glfw3.h>
#include <vulkan/vulkan.hpp>

#include "Pipeline.h"
#include "Swapchain.h"
#include "../glfw/Context.h"
#include "../glfw/Window.h"
#include "../util/Logger.h"

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

template<typename T, typename... Args>
static auto makeUniqueHandle(const T &t, Args &&... args) {
    return vk::UniqueHandle<T, VULKAN_HPP_DEFAULT_DISPATCHER_TYPE>(
        t, vk::UniqueHandleTraits<T, VULKAN_HPP_DEFAULT_DISPATCHER_TYPE>::deleter(std::forward<Args>(args)...));
}

static glfw::UniqueWindow createWindow(const glfw::WindowCreateInfo &window_create_info) {
    glfw::Context::init([](int error, const char *description) {
        Logger::error(std::format("GLFW error {:#010x}: {}", error, description));
    });

    return std::make_unique<glfw::Window>(window_create_info);
}

static vkb::Instance createInstance() {
    uint32_t required_glfw_extension_count = 0;
    auto required_glfw_extensions = glfwGetRequiredInstanceExtensions(&required_glfw_extension_count);
    auto instance_builder = vkb::InstanceBuilder{}
            .use_default_debug_messenger()
            .require_api_version(1, 3, 0)
            .enable_extension(vk::EXTDebugUtilsExtensionName)
            .enable_extension(vk::KHRGetSurfaceCapabilities2ExtensionName)
            .enable_extensions(required_glfw_extension_count, required_glfw_extensions);
#ifndef NDEBUG
    Logger::info("Using validation layers");
    instance_builder.enable_validation_layers(true);
    instance_builder.enable_layer("VK_LAYER_KHRONOS_synchronization2");
#endif

    auto instance_ret = instance_builder.build();
    if (!instance_ret) {
        Logger::fatal(instance_ret.error().message());
    }

    return instance_ret.value();
}

void printSystemInformation(const vk::Instance &instance) {
    Logger::info("Available layers:");
    for (auto layer_property: vk::enumerateInstanceLayerProperties()) {
        Logger::info(std::format(
            "- {}: {}", std::string(layer_property.layerName.data()), std::string(layer_property.description.data())
        ));
    }

    Logger::info("Available Devices:");
    for (auto device: instance.enumeratePhysicalDevices()) {
        auto props = device.getProperties();
        Logger::info(std::format("Name: {}", std::string(props.deviceName.data())));
        auto queues = device.getQueueFamilyProperties();
        for (size_t i = 0; i < queues.size(); i++) {
            auto &queue = queues.at(i);
            std::vector<std::string> caps;
            if (queue.queueFlags & vk::QueueFlagBits::eGraphics) caps.emplace_back("Graphics");
            if (queue.queueFlags & vk::QueueFlagBits::eTransfer) caps.emplace_back("Transfer");
            if (queue.queueFlags & vk::QueueFlagBits::eCompute) caps.emplace_back("Compute");
            if (queue.queueFlags & vk::QueueFlagBits::eOpticalFlowNV) caps.emplace_back("OpticalFlow");
            if (queue.queueFlags & vk::QueueFlagBits::eVideoDecodeKHR) caps.emplace_back("VideoDecode");
            if (queue.queueFlags & vk::QueueFlagBits::eVideoEncodeKHR) caps.emplace_back("VideoEncode");
            if (queue.queueFlags & vk::QueueFlagBits::eProtected) caps.emplace_back("Protected");
            if (queue.queueFlags & vk::QueueFlagBits::eSparseBinding) caps.emplace_back("SparseBinding");
            if (glfwGetPhysicalDevicePresentationSupport(instance, device, i)) caps.emplace_back("Present");
            Logger::info(std::format("  Queue Family: {} x {}", queue.queueCount, caps));
        }
    }
}

vkb::PhysicalDevice createPhysicalDevice(const vkb::Instance &instance, const vk::SurfaceKHR &surface) {
    auto phys_device_selector = vkb::PhysicalDeviceSelector(instance)
            .set_surface(surface)
            .set_required_features({
                .multiDrawIndirect = true,
                .depthClamp = true,
                .depthBiasClamp = true,
                .samplerAnisotropy = true,
            })
            .set_required_features_12({
                .drawIndirectCount = true,
                .descriptorIndexing = true,
                .shaderUniformBufferArrayNonUniformIndexing = true,
                .shaderSampledImageArrayNonUniformIndexing = true,
                .shaderStorageBufferArrayNonUniformIndexing = true,
                //.descriptorBindingUniformBufferUpdateAfterBind = true,
                .descriptorBindingSampledImageUpdateAfterBind = true,
                .descriptorBindingStorageBufferUpdateAfterBind = true,
                .descriptorBindingPartiallyBound = true,
                .descriptorBindingVariableDescriptorCount = false, // maybe later
                .runtimeDescriptorArray = true,
                .scalarBlockLayout = true,
                .uniformBufferStandardLayout = true,
                .timelineSemaphore = true,
                .bufferDeviceAddress = false, // maybe later
                .bufferDeviceAddressCaptureReplay = false,
                .bufferDeviceAddressMultiDevice = false,
            })
            .set_required_features_13({
                .robustImageAccess = true,
                .inlineUniformBlock = true,
                .synchronization2 = true,
                .dynamicRendering = true,
            })
            .add_required_extension(vk::KHRSwapchainExtensionName)
            .add_required_extension(vk::EXTMemoryBudgetExtensionName)
            .add_required_extension(vk::KHRSwapchainMutableFormatExtensionName)
            .add_required_extension_features(vk::PhysicalDeviceShaderDrawParametersFeatures{
                .shaderDrawParameters = true
            })
            .prefer_gpu_device_type(vkb::PreferredDeviceType::discrete)
            .allow_any_gpu_device_type(false)
            .require_present();

    auto phys_device_ret = phys_device_selector.select(vkb::DeviceSelectionMode::only_fully_suitable);
    if (!phys_device_ret) {
        Logger::fatal(phys_device_ret.error().message());
    }
    return phys_device_ret.value();
}

vkb::Device createDevice(const vkb::PhysicalDevice &physical_device) {
    vkb::DeviceBuilder device_builder{physical_device};
    auto device_ret = device_builder.build();
    if (!device_ret) {
        Logger::fatal(device_ret.error().message());
    }
    return device_ret.value();
}

vma::UniqueAllocator createVmaAllocator(const vk::Instance &instance, const vk::Device &device, const vk::PhysicalDevice &physical_device) {
    vma::VulkanFunctions vma_vulkan_functions = {
        .vkGetInstanceProcAddr = VULKAN_HPP_DEFAULT_DISPATCHER.vkGetInstanceProcAddr,
        .vkGetDeviceProcAddr = VULKAN_HPP_DEFAULT_DISPATCHER.vkGetDeviceProcAddr
    };
    return vma::createAllocatorUnique({
        .flags = vma::AllocatorCreateFlagBits::eExtMemoryBudget,
        .physicalDevice = physical_device,
        .device = device,
        .pVulkanFunctions = &vma_vulkan_functions,
        .instance = instance,
        .vulkanApiVersion = VK_API_VERSION_1_3,
    });
}

void createQueues(const vkb::Device &device, DeviceQueue &out_graphics_queue, DeviceQueue &out_transfer_queue, DeviceQueue &out_present_queue) {
    auto gq_ret = device.get_queue(vkb::QueueType::graphics);
    auto gq_family_ret = device.get_queue_index(vkb::QueueType::graphics);
    if (!gq_ret.has_value() || !gq_family_ret.has_value()) {
        Logger::fatal("failed to get graphics queue: " + gq_ret.error().message());
    }
    out_graphics_queue = {
        gq_ret.value(),
        gq_family_ret.value()
    };

    auto pq_ret = device.get_queue(vkb::QueueType::present);
    auto pq_family_ret = device.get_queue_index(vkb::QueueType::present);
    if (!pq_ret.has_value() || !pq_family_ret.has_value()) {
        Logger::fatal("failed to get present queue: " + pq_ret.error().message());
    }
    out_present_queue = {
        pq_ret.value(),
        pq_family_ret.value()
    };

    auto tq_ret = device.get_dedicated_queue(vkb::QueueType::transfer);
    auto tq_family_ret = device.get_dedicated_queue_index(vkb::QueueType::transfer);
    if (!tq_ret.has_value() || !tq_family_ret.has_value()) {
        Logger::fatal("failed to get transfer queue: " + tq_ret.error().message());
    }
    out_transfer_queue = {
        tq_ret.value(),
        tq_family_ret.value()
    };
}


VulkanContext VulkanContext::create(const glfw::WindowCreateInfo &window_create_info) {
    // Step 1: Create Window
    glfw::UniqueWindow window = createWindow(window_create_info);

    // Step 2: Create Vulkan Instance
    vkb::Instance instance = createInstance();
    // This loads the vulkan function pointers into the singleton dispatcher.
    // Thus we don't need to pass it to any vk::* calls.
    VULKAN_HPP_DEFAULT_DISPATCHER.init(instance.fp_vkGetInstanceProcAddr);
    VULKAN_HPP_DEFAULT_DISPATCHER.init(vk::Instance(instance.instance), instance.fp_vkGetInstanceProcAddr);
    printSystemInformation(instance.instance);

    // Step 3: Create Physical Device
    vk::UniqueSurfaceKHR surface = window->createWindowSurfaceKHRUnique(instance.instance);
    vkb::PhysicalDevice physical_device = createPhysicalDevice(instance, *surface);
    Logger::info("Using Physical Device: " + physical_device.name);

    // Step 4: Create Device
    vkb::Device device = createDevice(physical_device);
    VULKAN_HPP_DEFAULT_DISPATCHER.init(vk::Device(device.device));

    // Step 5: Create VMA Allocator
    vma::UniqueAllocator allocator = createVmaAllocator(instance.instance, device.device, physical_device.physical_device);

    // Step 6: Retrieve Queues
    DeviceQueue main_queue, transfer_queue, present_queue;
    createQueues(device, main_queue, transfer_queue, present_queue);

    // Step 7: Create Swapchain
    auto swapchain = std::make_unique<Swapchain>(
        device.device,
        physical_device.physical_device,
        *surface,
        *window,
        *allocator
    );

    return VulkanContext{
        makeUniqueHandle(vk::Instance(instance.instance), nullptr),
        makeUniqueHandle(vk::DebugUtilsMessengerEXT(instance.debug_messenger), instance.instance, nullptr),
        vk::PhysicalDevice(physical_device.physical_device),
        makeUniqueHandle(vk::Device(device.device), nullptr),
        std::move(allocator),
        std::move(window),
        std::move(surface),
        std::move(swapchain),
        main_queue,
        present_queue,
        transfer_queue,
    };
}

VulkanContext::VulkanContext(
    vk::UniqueInstance &&instance,
    vk::UniqueDebugUtilsMessengerEXT &&debug_messenger,
    const vk::PhysicalDevice &physical_device,
    vk::UniqueDevice &&device,
    vma::UniqueAllocator &&allocator,
    std::unique_ptr<glfw::Window> &&window,
    vk::UniqueSurfaceKHR &&surface,
    std::unique_ptr<Swapchain> &&swapchain,
    const DeviceQueue &main_queue,
    const DeviceQueue &present_queue,
    const DeviceQueue &transfer_queue)
    : mInstance(std::move(instance)),
      mDebugMessenger(std::move(debug_messenger)),
      mPhysicalDevice(physical_device),
      mDevice(std::move(device)),
      mAllocator(std::move(allocator)),
      mWindow(std::move(window)),
      mSurface(std::move(surface)),
      mSwapchain(std::move(swapchain)),
      mainQueue(main_queue),
      presentQueue(present_queue),
      transferQueue(transfer_queue) {
}

VulkanContext::~VulkanContext() = default;

VulkanContext::VulkanContext(VulkanContext &&other) noexcept
    : mInstance(std::move(other.mInstance)),
      mDebugMessenger(std::move(other.mDebugMessenger)),
      mPhysicalDevice(other.mPhysicalDevice),
      mDevice(std::move(other.mDevice)),
      mAllocator(std::move(other.mAllocator)),
      mWindow(std::move(other.mWindow)),
      mSurface(std::move(other.mSurface)),
      mSwapchain(std::move(other.mSwapchain)),
      mainQueue(other.mainQueue),
      presentQueue(other.presentQueue),
      transferQueue(other.transferQueue) {
}

VulkanContext &VulkanContext::operator=(VulkanContext &&other) noexcept {
    if (this == &other)
        return *this;
    mInstance = std::move(other.mInstance);
    mDebugMessenger = std::move(other.mDebugMessenger);
    mPhysicalDevice = other.mPhysicalDevice;
    mDevice = std::move(other.mDevice);
    mAllocator = std::move(other.mAllocator);
    mWindow = std::move(other.mWindow);
    mSurface = std::move(other.mSurface);
    mSwapchain = std::move(other.mSwapchain);
    mainQueue = other.mainQueue;
    presentQueue = other.presentQueue;
    transferQueue = other.transferQueue;
    return *this;
}
