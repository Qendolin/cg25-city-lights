#pragma once

#include <filesystem>
#include <glm/glm.hpp>
#include <vulkan-memory-allocator-hpp/vk_mem_alloc.hpp>

#include "../backend/Descriptors.h"
#include "../util/math.h"

namespace gltf {
    struct Scene;
    class Loader;
}


namespace scene {
    struct SceneDescriptorLayout : DescriptorSetLayout {
        static constexpr StorageBufferBinding SectionBuffer{0, vk::ShaderStageFlagBits::eAllGraphics};
        static constexpr StorageBufferBinding InstanceBuffer{1, vk::ShaderStageFlagBits::eAllGraphics};
        static constexpr StorageBufferBinding MaterialBuffer{2, vk::ShaderStageFlagBits::eAllGraphics};

        SceneDescriptorLayout() = default;

        explicit SceneDescriptorLayout(const vk::Device& device) {
            create(device, {}, SectionBuffer, InstanceBuffer, MaterialBuffer);
        }
    };

    struct GpuData {
        GpuData() = default;
        ~GpuData() = default;

        GpuData(const GpuData &other) = delete;
        GpuData &operator=(const GpuData &other) = delete;

        GpuData(GpuData &&other) noexcept = default;
        GpuData &operator=(GpuData &&other) noexcept = default;

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

        vma::UniqueBuffer materials;
        vma::UniqueAllocation materialsAlloc;

        uint32_t drawCommandCount;
        vma::UniqueBuffer drawCommands;
        vma::UniqueAllocation drawCommandsAlloc;

        /// <summary>
        /// A helper function to write the scene's descriptor set.
        /// </summary>
        void writeDescriptorSet(const vk::Device &device, const DescriptorSet &descriptor) const;
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
        BoundingBox bounds;
    };

    // Not much right now, but can be expanded as needed
    struct CpuData {
        std::vector<Instance> instances;
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
                const vk::CommandPool &transferCommandPool,
                const vk::Queue &transferQueue
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
        vk::CommandPool mTransferCommandPool;
        vk::Queue mTransferQueue;
    };
} // namespace scene
