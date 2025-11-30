#pragma once

#include <filesystem>
#include <glm/glm.hpp>
#include <vulkan-memory-allocator-hpp/vk_mem_alloc.hpp>

#include "../backend/Descriptors.h"
#include "../backend/DeviceQueue.h"
#include "../backend/Image.h"
#include "../entity/Light.h"
#include "../util/math.h"
#include "gpu_types.h"


namespace gltf {
    struct Scene;
    class Loader;
}

namespace scene {
    struct GpuData {
        GpuData() = default;
        ~GpuData() = default;

        GpuData(const GpuData &other) = delete;
        GpuData &operator=(const GpuData &other) = delete;

        GpuData(GpuData &&other) noexcept = default;
        GpuData &operator=(GpuData &&other) noexcept = default;

        vk::UniqueSampler sampler;
        std::vector<Image> images;
        std::vector<ImageView> views;

        vma::UniqueBuffer positions;
        vma::UniqueAllocation positionsAlloc;
        vma::UniqueBuffer normals;
        vma::UniqueAllocation normalsAlloc;
        vma::UniqueBuffer tangents;
        vma::UniqueAllocation tangentsAlloc;
        vma::UniqueBuffer texcoords;
        vma::UniqueAllocation texcoordsAlloc;
        vma::UniqueBuffer indices;
        vma::UniqueAllocation indicesAlloc;

        vma::UniqueBuffer sections;
        vma::UniqueAllocation sectionsAlloc;
        vma::UniqueBuffer instances;
        vma::UniqueAllocation instancesAlloc;
        vma::UniqueBuffer boundingBoxes;
        vma::UniqueAllocation boundingBoxesAlloc;

        vma::UniqueBuffer materials;
        vma::UniqueAllocation materialsAlloc;

        vma::UniqueBuffer pointLights;
        vma::UniqueAllocation pointLightsAlloc;
        vma::UniqueBuffer spotLights;
        vma::UniqueAllocation spotLightsAlloc;

        SceneDescriptorLayout sceneDescriptorLayout = {};
        vk::UniqueDescriptorPool sceneDescriptorPool = {};
        DescriptorSet sceneDescriptor = {};

        uint32_t drawCommandCount = 0;
        vma::UniqueBuffer drawCommands;
        vma::UniqueAllocation drawCommandsAlloc;
    };

    struct Instance {
        /// <summary>
        /// The unique name of this instance.
        /// </summary>
        std::string name;
        /// <summary>
        /// The transformation matrix of this instance.
        /// </summary>
        glm::mat4 transform;
        /// <summary>
        /// The bounds of this instances mesh in local space.
        /// </summary>
        util::BoundingBox bounds;
    };

    struct InstanceAnimation {
        std::vector<float> translation_timestamps;
        std::vector<float> rotation_timestamps;
        std::vector<glm::vec3> translations;
        std::vector<glm::vec4> rotations;
    };

    struct CpuData {
        std::vector<Instance> instances;
        // New, TODO: Summary
        std::vector<std::size_t> animated_instances;
        std::vector<InstanceAnimation> instance_animations;
    };

    class Scene {
    public:
        Scene(); // needed to make the fucking compiler happy
        Scene(CpuData &&cpu_data, GpuData &&gpu_data);

        Scene(const Scene &other) = delete;
        Scene(Scene &&other) noexcept = default;
        Scene &operator=(const Scene &other) = delete;
        Scene &operator=(Scene &&other) noexcept = default;

        ~Scene();

        [[nodiscard]] const CpuData &cpu() const { return mCpuData; }
        [[nodiscard]] const GpuData &gpu() const { return mGpuData; }

    private:
        CpuData mCpuData;
        GpuData mGpuData;
    };


    class Loader {
    public:
        Loader(
                const gltf::Loader *loader,
                const vma::Allocator &allocator,
                const vk::Device &device,
                const vk::PhysicalDevice &physical_device,
                const DeviceQueue &transferQueue,
                const DeviceQueue &graphicsQueue
        );

        /// <summary>
        /// Loads a scene from the given path.
        /// </summary>
        /// <param name="path">The path to the glTF file.</param>
        [[nodiscard]] Scene load(const std::filesystem::path &path) const;

    private:
        [[nodiscard]] CpuData createCpuData(const gltf::Scene &scene_data) const;

        [[nodiscard]] GpuData createGpuData(const gltf::Scene &scene_data) const;

        const gltf::Loader *mLoader;
        vma::Allocator mAllocator;
        vk::Device mDevice;
        vk::PhysicalDevice mPhysicalDevice;
        DeviceQueue mTransferQueue;
        DeviceQueue mGraphicsQueue;
    };
} // namespace scene
