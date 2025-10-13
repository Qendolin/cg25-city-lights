#include "Scene.h"

#include "../backend/StagingBuffer.h"
#include "Gltf.h"
#include "gpu_types.h"

namespace scene {

    void GpuData::writeDescriptorSet(const vk::Device &device, const DescriptorSet &descriptor) const {
        device.updateDescriptorSets(
                {
                    descriptor.write(
                            SceneDescriptorLayout::SectionBuffer,
                            vk::DescriptorBufferInfo{.buffer = *sections, .offset = 0, .range = vk::WholeSize}
                    ),
                    descriptor.write(
                            SceneDescriptorLayout::InstanceBuffer,
                            vk::DescriptorBufferInfo{.buffer = *instances, .offset = 0, .range = vk::WholeSize}
                    ),
                    descriptor.write(
                            SceneDescriptorLayout::MaterialBuffer,
                            vk::DescriptorBufferInfo{.buffer = *materials, .offset = 0, .range = vk::WholeSize}
                    ),
                },
                {}
        );
    }

    Scene::Scene() = default;
    Scene::Scene(CpuData &&cpu_data, GpuData &&gpu_data)
        : mCpuData(std::move(cpu_data)), mGpuData(std::move(gpu_data)) {}

    Scene::~Scene() = default;

    Loader::Loader(
            const gltf::Loader *loader,
            const vma::Allocator &allocator,
            const vk::Device &device,
            const vk::CommandPool &transferCommandPool,
            const vk::Queue &transferQueue
    )
        : mLoader(loader),
          mAllocator(allocator),
          mDevice(device),
          mTransferCommandPool(transferCommandPool),
          mTransferQueue(transferQueue) {}

    Scene Loader::load(const std::filesystem::path &path) const {
        auto gltf_scene = mLoader->load(path);
        auto cpu_data = createCpuData(gltf_scene);
        auto gpu_data = createGpuData(gltf_scene);

        return Scene(std::move(cpu_data), std::move(gpu_data));
    }

    CpuData Loader::createCpuData(const gltf::Scene &scene_data) const {
        CpuData result;

        result.instances.reserve(scene_data.nodes.size());
        for (const auto &node: scene_data.nodes) {
            util::BoundingBox bounds = {};
            if (node.mesh != UINT32_MAX) {
                bounds = scene_data.meshes[node.mesh].bounds;
            }
            result.instances.emplace_back() = {
                .name = node.name,
                .transform = node.transform,
                .bounds = bounds,
            };
        }

        return result;
    }

    GpuData Loader::createGpuData(const gltf::Scene &scene_data) const {
        GpuData result;
        StagingBuffer staging = {mAllocator, mDevice, mTransferCommandPool};

        std::tie(result.positions, result.positionsAlloc) =
                staging.upload(scene_data.vertex_position_data, vk::BufferUsageFlagBits::eVertexBuffer);
        std::tie(result.normals, result.normalsAlloc) =
                staging.upload(scene_data.vertex_normal_data, vk::BufferUsageFlagBits::eVertexBuffer);
        std::tie(result.tangents, result.tangentsAlloc) =
                staging.upload(scene_data.vertex_tangent_data, vk::BufferUsageFlagBits::eVertexBuffer);
        std::tie(result.texcoords, result.texcoordsAlloc) =
                staging.upload(scene_data.vertex_texcoord_data, vk::BufferUsageFlagBits::eVertexBuffer);
        std::tie(result.indices, result.indicesAlloc) =
                staging.upload(scene_data.index_data, vk::BufferUsageFlagBits::eIndexBuffer);

        std::vector<SectionBlock> section_blocks;
        section_blocks.reserve(scene_data.sections.size());
        std::vector<vk::DrawIndexedIndirectCommand> draw_commands;
        draw_commands.reserve(scene_data.sections.size());
        for (size_t i = 0; i < scene_data.sections.size(); i++) {
            const auto &section = scene_data.sections[i];
            draw_commands.emplace_back() = vk::DrawIndexedIndirectCommand{
                .indexCount = section.indexCount,
                .instanceCount = 1,
                .firstIndex = section.indexOffset,
                .vertexOffset = section.vertexOffset,
                .firstInstance = static_cast<uint32_t>(i),
            };
            section_blocks.emplace_back() = {.instance = section.node, .material = section.material};
        }
        std::tie(result.sections, result.sectionsAlloc) =
                staging.upload(section_blocks, vk::BufferUsageFlagBits::eStorageBuffer);

        std::vector<InstanceBlock> instance_blocks;
        instance_blocks.reserve(std::ranges::count_if(scene_data.nodes, [](const auto &node) {
            return node.mesh != UINT32_MAX;
        }));
        for (const auto &node: scene_data.nodes) {
            if (node.mesh == UINT32_MAX)
                continue;
            instance_blocks.emplace_back() = {.transform = node.transform};
        }
        std::tie(result.instances, result.instancesAlloc) =
                staging.upload(instance_blocks, vk::BufferUsageFlagBits::eStorageBuffer);

        std::tie(result.drawCommands, result.drawCommandsAlloc) =
                staging.upload(draw_commands, vk::BufferUsageFlagBits::eIndirectBuffer);
        result.drawCommandCount = static_cast<uint32_t>(draw_commands.size());

        std::vector<MaterialBlock> material_blocks;
        material_blocks.reserve(scene_data.materials.size());
        for (const auto &material: scene_data.materials) {
            material_blocks.emplace_back() = {
                .albedoFactors = material.albedoFactor,
                .mrnFactors = glm::vec4{material.metalnessFactor, material.roughnessFactor, material.normalFactor, 1.0f},
            };
        }
        std::tie(result.materials, result.materialsAlloc) =
                staging.upload(material_blocks, vk::BufferUsageFlagBits::eStorageBuffer);

        staging.submit(mTransferQueue);

        return result;
    }

} // namespace scene
