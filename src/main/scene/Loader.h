#pragma once

#include <filesystem>
#include <vulkan-memory-allocator-hpp/vk_mem_alloc.hpp>

#include "../backend/DeviceQueue.h"
#include "Scene.h"


namespace gltf {
    struct Scene;
    class Loader;
}
namespace scene {
    class Loader {
    public:
        Loader(const vma::Allocator &allocator,
               const vk::Device &device,
               const vk::PhysicalDevice &physical_device,
               const DeviceQueue &transferQueue,
               const DeviceQueue &graphicsQueue);

        /// <summary>
        /// Loads a scene from the given path.
        /// </summary>
        /// <param name="path">The path to the glTF file.</param>
        [[nodiscard]] Scene load(const std::filesystem::path &path) const;

    private:
        [[nodiscard]] CpuData createCpuData(const gltf::Scene &scene_data) const;

        [[nodiscard]] GpuData createGpuData(const gltf::Scene &scene_data) const;

        vma::Allocator mAllocator;
        vk::Device mDevice;
        vk::PhysicalDevice mPhysicalDevice;
        DeviceQueue mTransferQueue;
        DeviceQueue mGraphicsQueue;
    };
} // namespace scene
