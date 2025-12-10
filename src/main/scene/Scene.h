#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <vector>
#include <vulkan-memory-allocator-hpp/vk_mem_alloc.hpp>

#include "../backend/Descriptors.h"
#include "../backend/Image.h"
#include "../util/math.h"
#include "gpu_types.h"

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

        vma::UniqueBuffer uberLights;
        vma::UniqueAllocation uberLightsAlloc;

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

    /// <summary>
    /// Stores the respective timestamps if translation or rotation animation data exists and the
    /// associated animation data at the same index.
    /// </summary>
    struct InstanceAnimation {
        std::vector<float> translation_timestamps;
        std::vector<float> rotation_timestamps;
        std::vector<glm::vec3> translations;
        std::vector<glm::quat> rotations;
    };

    struct CpuData {
        /// <summary>
        /// The instances present in the scene.
        /// </summary>
        std::vector<Instance> instances;

        /// <summary>
        /// Maps the animation indices to the indices of instances in the instances vector.
        /// </summary>
        std::vector<std::size_t> animated_instances;

        /// <summary>
        /// The data of n animations for the last n instances in the instances vector.
        /// </summary>
        std::vector<InstanceAnimation> instance_animations;
    };

    class Scene {
    public:
        Scene() = default;
        Scene(CpuData &&cpu_data, GpuData &&gpu_data) : mCpuData(std::move(cpu_data)), mGpuData(std::move(gpu_data)) {}

        ~Scene() = default;

        Scene(const Scene &other) = delete;
        Scene(Scene &&other) noexcept = default;
        Scene &operator=(const Scene &other) = delete;
        Scene &operator=(Scene &&other) noexcept = default;

        [[nodiscard]] const CpuData &cpu() const { return mCpuData; }
        [[nodiscard]] const GpuData &gpu() const { return mGpuData; }

    private:
        CpuData mCpuData;
        GpuData mGpuData;
    };
} // namespace scene
