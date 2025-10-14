#include "Scene.h"

#include "../backend/StagingBuffer.h"
#include "Gltf.h"
#include "gpu_types.h"

namespace scene {

    Scene::Scene() = default;
    Scene::Scene(CpuData &&cpu_data, GpuData &&gpu_data)
        : mCpuData(std::move(cpu_data)), mGpuData(std::move(gpu_data)) {}

    Scene::~Scene() = default;

    Loader::Loader(
            const gltf::Loader *loader,
            const vma::Allocator &allocator,
            const vk::Device &device,
            const vk::PhysicalDevice &physical_device,
            const DeviceQueue &transferQueue,
            const DeviceQueue &graphicsQueue,
            const DescriptorAllocator &descriptor_allocator
    )
        : mLoader(loader),
          mAllocator(allocator),
          mDevice(device),
          mPhysicalDevice(physical_device),
          mTransferQueue(transferQueue),
          mGraphicsQueue(graphicsQueue),
          mDescriptorAllocator(descriptor_allocator) {}

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
        auto graphics_cmd_pool = mDevice.createCommandPoolUnique(
                {.flags = vk::CommandPoolCreateFlagBits::eTransient, .queueFamilyIndex = mGraphicsQueue}
        );
        auto transfer_cmd_pool = mDevice.createCommandPoolUnique(
                {.flags = vk::CommandPoolCreateFlagBits::eTransient, .queueFamilyIndex = mTransferQueue}
        );

        auto graphics_cmds =
                mDevice.allocateCommandBuffers({.commandPool = *graphics_cmd_pool, .commandBufferCount = 1}).at(0);
        graphics_cmds.begin({.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit});

        GpuData result;
        StagingBuffer staging = {mAllocator, mDevice, *transfer_cmd_pool};

        result.sceneDescriptorLayout = SceneDescriptorLayout(mDevice);
        result.sceneDescriptor = mDescriptorAllocator.allocate(result.sceneDescriptorLayout);

        float max_anisotropy = mPhysicalDevice.getProperties().limits.maxSamplerAnisotropy;
        result.sampler = mDevice.createSamplerUnique(
                {.magFilter = vk::Filter::eLinear,
                 .minFilter = vk::Filter::eLinear,
                 .mipmapMode = vk::SamplerMipmapMode::eLinear,
                 .anisotropyEnable = true,
                 .maxAnisotropy = max_anisotropy,
                 .maxLod = vk::LodClampNone,
                 .borderColor = vk::BorderColor::eFloatOpaqueBlack}
        );

        result.images.reserve(scene_data.images.size());
        result.views.reserve(scene_data.images.size());
        // since some images will be skipped, the indices need to be mapped
        std::vector<size_t> image_index;
        for (const auto &image_data: scene_data.images) {
            // image isn't used by any material
            if (image_data.format == vk::Format::eUndefined) {
                image_index.emplace_back(-1);
                continue;
            }
            size_t index = result.images.size();
            image_index.emplace_back(index);

            auto create_info = ImageCreateInfo::from(image_data);
            create_info.usage |= vk::ImageUsageFlagBits::eSampled;

            Image &image = result.images.emplace_back();
            image = Image::create(staging.allocator(), create_info);
            vk::Buffer staged_buffer = staging.stage(image_data.pixels);
            image.load(staging.commands(), 0, {}, staged_buffer);
            image.transfer(staging.commands(), graphics_cmds, mTransferQueue, mGraphicsQueue);
            image.generateMipmaps(graphics_cmds);
            image.barrier(graphics_cmds, ImageResourceAccess::FragmentShaderRead);

            vk::UniqueImageView &view = result.views.emplace_back();
            view = image.createDefaultView(mDevice);

            mDevice.updateDescriptorSets(
                    {result.sceneDescriptor.write(
                            SceneDescriptorLayout::ImageSamplers,
                            vk::DescriptorImageInfo{
                                .sampler = *result.sampler, .imageView = *view, .imageLayout = vk::ImageLayout::eReadOnlyOptimal
                            },
                            static_cast<uint32_t>(index)
                    )},
                    {}
            );
        }

        graphics_cmds.end();

        auto graphics_queue_fence = mDevice.createFenceUnique({});
        auto image_transfer_semaphore = mDevice.createSemaphoreUnique({});
        staging.submit(mTransferQueue, vk::SubmitInfo().setSignalSemaphores(*image_transfer_semaphore));
        vk::PipelineStageFlags semaphore_stage_mask = vk::PipelineStageFlagBits::eTopOfPipe;
        mGraphicsQueue.queue.submit(
                {vk::SubmitInfo()
                         .setWaitSemaphores(*image_transfer_semaphore)
                         .setCommandBuffers(graphics_cmds)
                         .setWaitDstStageMask(semaphore_stage_mask)},
                *graphics_queue_fence
        );

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
        for (size_t i = 0; i < scene_data.materials.size(); i++) {
            const auto &material = scene_data.materials[i];
            uint32_t albedo_texture_index = material.albedoTexture == -1 ? 0xffff : image_index[material.albedoTexture];
            uint32_t normal_texture_index = material.normalTexture == -1 ? 0xffff : image_index[material.normalTexture];
            uint32_t orm_texture_index = material.ormTexture == -1 ? 0xffff : image_index[material.ormTexture];
            material_blocks.emplace_back() = {
                .albedoFactors = material.albedoFactor,
                .rmnFactors = glm::vec4{material.roughnessFactor, material.metalnessFactor, material.normalFactor, 1.0f},
                .packedImageIndices0 = albedo_texture_index & 0xffff | (normal_texture_index & 0xffff) << 16,
                .packedImageIndices1 = orm_texture_index & 0xffff,
            };
        }
        std::tie(result.materials, result.materialsAlloc) =
                staging.upload(material_blocks, vk::BufferUsageFlagBits::eStorageBuffer);


        mDevice.updateDescriptorSets(
                {
                    result.sceneDescriptor.write(
                            SceneDescriptorLayout::SectionBuffer,
                            vk::DescriptorBufferInfo{.buffer = *result.sections, .offset = 0, .range = vk::WholeSize}
                    ),
                    result.sceneDescriptor.write(
                            SceneDescriptorLayout::InstanceBuffer,
                            vk::DescriptorBufferInfo{.buffer = *result.instances, .offset = 0, .range = vk::WholeSize}
                    ),
                    result.sceneDescriptor.write(
                            SceneDescriptorLayout::MaterialBuffer,
                            vk::DescriptorBufferInfo{.buffer = *result.materials, .offset = 0, .range = vk::WholeSize}
                    ),
                },
                {}
        );

        staging.submit(mTransferQueue);

        while (mDevice.waitForFences(*graphics_queue_fence, true, UINT64_MAX) == vk::Result::eTimeout) {
        }

        return result;
    }

} // namespace scene
