#pragma once

#include <filesystem>
#include <string>
#include <vector>
#include <vulkan-memory-allocator-hpp/vk_mem_alloc.hpp>

#include "../backend/DeviceQueue.h"
#include "Scene.h"

namespace gltf {
    struct Scene;
    struct Animation;
    class Loader;
}
namespace scene {
    struct InstanceAnimation;

    class Loader {
    private:
        static constexpr int UNIFORM_BUFFER_POOL_SIZE{1024};
        static constexpr int COMBINED_IMAGE_SAMPLER_POOL_SIZE{4096};
        static constexpr int STORAGE_BUFFER_POOL_SIZE{1024};
        inline static const std::string DEBUG_NAME_PREFIX{"scene_"};

        vma::Allocator mAllocator;
        vk::Device mDevice;
        vk::PhysicalDevice mPhysicalDevice;
        DeviceQueue mTransferQueue;
        DeviceQueue mGraphicsQueue;

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

        [[nodiscard]] static InstanceAnimation createInstanceAnimation(const gltf::Animation &animation_data);

        void createGpuDataInitDescriptorPool(GpuData &gpu_data) const;

        void createGpuDataInitDescriptorSet(GpuData &gpu_data) const;

        void createGpuDataInitSampler(GpuData &gpu_data) const;

        // Returns a mapping from gltf images in order to their in-application index because some images might be skipped
        [[nodiscard]] std::vector<uint32_t> createGpuDataInitImages(
                const gltf::Scene &scene_data, const vk::CommandBuffer &graphics_cmds, StagingBuffer &staging, GpuData &gpu_data
        ) const;

        void createGpuDataInitVertices(const gltf::Scene &scene_data, StagingBuffer &staging, GpuData &gpu_data) const;

        [[nodiscard]] std::vector<glm::uint> createGpuDataInitInstances(
                const gltf::Scene &scene_data, StagingBuffer &staging, GpuData &gpu_data
        ) const;

        void createGpuDataInitSections(
                const gltf::Scene &scene_data,
                StagingBuffer &staging,
                const std::vector<glm::uint> &node_instance_map,
                GpuData &gpu_data
        ) const;

        void createGpuDataInitMaterials(
                const gltf::Scene &scene_data, StagingBuffer &staging, const std::vector<uint32_t> &image_indices, GpuData &gpu_data
        ) const;

        void createGpuDataInitLights(const gltf::Scene &scene_data, StagingBuffer &staging, GpuData &gpu_data) const;

        void createGpuDataUpdateDescriptorSet(GpuData &gpu_data) const;

        template<typename T>
        void uploadBufferWithDebugName(
                StagingBuffer &staging,
                T &&src,
                vk::BufferUsageFlags usage,
                const std::string &debugName,
                vma::UniqueBuffer &outBuffer,
                vma::UniqueAllocation &outAlloc
        ) const;
    };
} // namespace scene
