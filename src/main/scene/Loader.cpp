#include "Loader.h"

#include <algorithm>

#include "../backend/StagingBuffer.h"
#include "../debug/Annotation.h"
#include "Gltf.h"
#include "gpu_types.h"

namespace scene {
    Loader::Loader(
            const gltf::Loader *loader,
            const vma::Allocator &allocator,
            const vk::Device &device,
            const vk::PhysicalDevice &physical_device,
            const DeviceQueue &transferQueue,
            const DeviceQueue &graphicsQueue
    )
        : mLoader(loader),
          mAllocator(allocator),
          mDevice(device),
          mPhysicalDevice(physical_device),
          mTransferQueue(transferQueue),
          mGraphicsQueue(graphicsQueue) {}

    Scene Loader::load(const std::filesystem::path &path) const {
        gltf::Scene gltf_scene = mLoader->load(path);
        CpuData cpu_data = createCpuData(gltf_scene);
        GpuData gpu_data = createGpuData(gltf_scene);

        return {std::move(cpu_data), std::move(gpu_data)};
    }

    CpuData Loader::createCpuData(const gltf::Scene &scene_data) const {
        CpuData result;
        const std::size_t node_count = scene_data.nodes.size();
        const std::size_t animated_node_count = std::ranges::count_if(scene_data.nodes, [](const gltf::Node &node) {
            return node.animation != UINT32_MAX;
        });
        const std::size_t static_node_count = node_count - animated_node_count;

        result.instances.resize(node_count);
        result.animated_instances.reserve(animated_node_count);
        result.instance_animations.reserve(animated_node_count);

        std::size_t next_static_inst_idx{0};
        std::size_t next_animated_inst_idx{static_node_count};

        for (const gltf::Node &node: scene_data.nodes) {
            util::BoundingBox bounds = {};
            if (node.mesh != UINT32_MAX)
                bounds = scene_data.meshes[node.mesh].bounds;

            std::size_t instance_idx;

            if (node.animation == UINT32_MAX)
                instance_idx = next_static_inst_idx++;
            else {
                instance_idx = next_animated_inst_idx++;
                result.animated_instances.push_back(instance_idx);

                const gltf::Animation &gltf_anim = scene_data.animations[node.animation];

                InstanceAnimation instance_anim{
                    gltf_anim.translation_timestamps,
                    gltf_anim.rotation_timestamps,
                    gltf_anim.translations,
                    gltf_anim.rotations,
                };

                result.instance_animations.push_back(std::move(instance_anim));
            }

            result.instances[instance_idx] = {
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

        {
            result.sceneDescriptorLayout = SceneDescriptorLayout(mDevice);
            // Bindless gets it's own pool because of it's pool size requirements
            std::array pool_sizes = {
                vk::DescriptorPoolSize{vk::DescriptorType::eUniformBuffer, 1024},
                vk::DescriptorPoolSize{vk::DescriptorType::eCombinedImageSampler, 65536},
                vk::DescriptorPoolSize{vk::DescriptorType::eStorageBuffer, 1024},
            };
            result.sceneDescriptorPool = mDevice.createDescriptorPoolUnique(vk::DescriptorPoolCreateInfo{
                .maxSets = 1,
            }
                                                                                    .setPoolSizes(pool_sizes));
            util::setDebugName(mDevice, *result.sceneDescriptorPool, "scene_descriptor_pool");

            vk::DescriptorSetLayout vk_layout = result.sceneDescriptorLayout;
            result.sceneDescriptor = DescriptorSet(mDevice.allocateDescriptorSets(
                    {.descriptorPool = *result.sceneDescriptorPool, .descriptorSetCount = 1, .pSetLayouts = &vk_layout}
            )[0]);
            util::setDebugName(mDevice, vk::DescriptorSet(result.sceneDescriptor), "scene_descriptor_set");
        }

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
        util::setDebugName(mDevice, *result.sampler, "scene_sampler");

        result.images.reserve(scene_data.images.size());
        result.views.reserve(scene_data.images.size());
        // since some images will be skipped, the indices need to be mapped
        std::vector<uint32_t> image_index;
        for (const auto &image_data: scene_data.images) {
            // image isn't used by any material
            if (image_data.format == vk::Format::eUndefined) {
                image_index.emplace_back(-1);
                continue;
            }
            uint32_t index = static_cast<uint32_t>(result.images.size());
            image_index.emplace_back(index);

            ImageCreateInfo create_info = {
                .format = image_data.format,
                .aspects = vk::ImageAspectFlagBits::eColor,
                .width = image_data.width,
                .height = image_data.height,
                .levels = UINT32_MAX,
                .usage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferSrc |
                         vk::ImageUsageFlagBits::eTransferDst,
            };
            Image &image = result.images.emplace_back();
            image = Image::create(staging.allocator(), create_info);
            util::setDebugName(mDevice, *image.image, "scene_image");

            vk::Buffer staged_buffer = staging.stage(image_data.pixels);
            image.load(staging.commands(), 0, {}, staged_buffer);
            image.transfer(staging.commands(), graphics_cmds, mTransferQueue, mGraphicsQueue);
            image.generateMipmaps(graphics_cmds);
            image.barrier(graphics_cmds, ImageResourceAccess::FragmentShaderReadOptimal);

            ImageView &view = result.views.emplace_back();
            view = ImageView::create(mDevice, image);
            util::setDebugName(mDevice, *view.view, "scene_image_view");

            mDevice.updateDescriptorSets(
                    {result.sceneDescriptor.write(
                            SceneDescriptorLayout::ImageSamplers,
                            vk::DescriptorImageInfo{
                                .sampler = *result.sampler, .imageView = view, .imageLayout = vk::ImageLayout::eReadOnlyOptimal
                            },
                            index
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
        util::setDebugName(mDevice, *result.positions, "scene_vertex_positions");
        std::tie(result.normals, result.normalsAlloc) =
                staging.upload(scene_data.vertex_normal_data, vk::BufferUsageFlagBits::eVertexBuffer);
        util::setDebugName(mDevice, *result.normals, "scene_vertex_normals");
        std::tie(result.tangents, result.tangentsAlloc) =
                staging.upload(scene_data.vertex_tangent_data, vk::BufferUsageFlagBits::eVertexBuffer);
        util::setDebugName(mDevice, *result.tangents, "scene_vertex_tangents");
        std::tie(result.texcoords, result.texcoordsAlloc) =
                staging.upload(scene_data.vertex_texcoord_data, vk::BufferUsageFlagBits::eVertexBuffer);
        util::setDebugName(mDevice, *result.texcoords, "scene_vertex_texcoords");
        std::tie(result.indices, result.indicesAlloc) =
                staging.upload(scene_data.index_data, vk::BufferUsageFlagBits::eIndexBuffer);
        util::setDebugName(mDevice, *result.indices, "scene_vertex_indices");

        std::vector<InstanceBlock> instance_blocks;
        instance_blocks.reserve(std::ranges::count_if(scene_data.nodes, [](const auto &node) {
            return node.mesh != UINT32_MAX;
        }));
        // maps node index to instance index
        std::vector<glm::uint> node_instance_map(scene_data.nodes.size());
        for (size_t i = 0; i < scene_data.nodes.size(); i++) {
            const auto &node = scene_data.nodes[i];
            if (node.mesh == UINT32_MAX) {
                node_instance_map[i] = UINT32_MAX;
                continue;
            }
            node_instance_map[i] = static_cast<glm::uint>(instance_blocks.size());
            instance_blocks.emplace_back() = {.transform = node.transform};
        }
        std::tie(result.instances, result.instancesAlloc) =
                staging.upload(instance_blocks, vk::BufferUsageFlagBits::eStorageBuffer);
        util::setDebugName(mDevice, *result.instances, "scene_instances");

        std::vector<SectionBlock> section_blocks;
        section_blocks.reserve(scene_data.sections.size());
        std::vector<vk::DrawIndexedIndirectCommand> draw_commands;
        draw_commands.reserve(scene_data.sections.size());
        std::vector<BoundingBoxBlock> bounding_box_blocks;
        bounding_box_blocks.reserve(scene_data.bounds.size());
        for (size_t i = 0; i < scene_data.sections.size(); i++) {
            const auto &section = scene_data.sections[i];
            draw_commands.emplace_back() = vk::DrawIndexedIndirectCommand{
                .indexCount = section.indexCount,
                .instanceCount = 1,
                .firstIndex = section.indexOffset,
                .vertexOffset = section.vertexOffset,
                .firstInstance = static_cast<uint32_t>(i),
            };
            section_blocks.emplace_back() = {.instance = node_instance_map[section.node], .material = section.material};
            const auto &bounds = scene_data.bounds[section.bounds];
            bounding_box_blocks.emplace_back() = {.min = glm::vec4(bounds.min, 0.0f), .max = glm::vec4(bounds.max, 0.0f)};
        }
        std::tie(result.drawCommands, result.drawCommandsAlloc) = staging.upload(
                draw_commands, vk::BufferUsageFlagBits::eIndirectBuffer | vk::BufferUsageFlagBits::eStorageBuffer
        );
        util::setDebugName(mDevice, *result.drawCommands, "scene_draw_commands");
        result.drawCommandCount = static_cast<uint32_t>(draw_commands.size());
        std::tie(result.sections, result.sectionsAlloc) =
                staging.upload(section_blocks, vk::BufferUsageFlagBits::eStorageBuffer);
        util::setDebugName(mDevice, *result.sections, "scene_sections");
        std::tie(result.boundingBoxes, result.boundingBoxesAlloc) =
                staging.upload(bounding_box_blocks, vk::BufferUsageFlagBits::eStorageBuffer);
        util::setDebugName(mDevice, *result.boundingBoxes, "scene_bounding_boxes");


        std::vector<MaterialBlock> material_blocks;
        material_blocks.reserve(scene_data.materials.size());
        for (const auto &material: scene_data.materials) {
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
        util::setDebugName(mDevice, *result.materials, "scene_materials");

        std::vector<PointLightBlock> point_light_blocks;
        point_light_blocks.reserve(scene_data.pointLights.size());
        for (const auto &light: scene_data.pointLights) {
            point_light_blocks.emplace_back() = {
                .radiance = glm::vec4(light.radiance(), 0.0f),
                .position = glm::vec4(light.position, 0.0f),
            };
        }
        std::tie(result.pointLights, result.pointLightsAlloc) =
                staging.upload(point_light_blocks, vk::BufferUsageFlagBits::eStorageBuffer);
        util::setDebugName(mDevice, *result.pointLights, "scene_point_lights");

        std::vector<SpotLightBlock> spot_light_blocks;
        spot_light_blocks.reserve(scene_data.spotLights.size());
        for (const auto &light: scene_data.spotLights) {
            float angle_scale = 1.0f / std::max(
                                               0.001f, glm::cos(glm::radians(light.innerConeAngle)) -
                                                               glm::cos(glm::radians(light.outerConeAngle))
                                       );
            spot_light_blocks.emplace_back() = {
                .radiance = glm::vec4(light.radiance(), 0.0f),
                .position = glm::vec4(light.position, 0.0f),
                .direction = glm::vec4(light.direction(), 0.0f),
                .coneAngleScale = angle_scale,
                .coneAngleOffset = -glm::cos(glm::radians(light.outerConeAngle)) * angle_scale,
            };
        }
        std::tie(result.spotLights, result.spotLightsAlloc) =
                staging.upload(spot_light_blocks, vk::BufferUsageFlagBits::eStorageBuffer);
        util::setDebugName(mDevice, *result.spotLights, "scene_spot_lights");

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
                    result.sceneDescriptor.write(
                            SceneDescriptorLayout::PointLightBuffer,
                            vk::DescriptorBufferInfo{.buffer = *result.pointLights, .offset = 0, .range = vk::WholeSize}
                    ),
                    result.sceneDescriptor.write(
                            SceneDescriptorLayout::SpotLightBuffer,
                            vk::DescriptorBufferInfo{.buffer = *result.spotLights, .offset = 0, .range = vk::WholeSize}
                    ),
                    result.sceneDescriptor.write(
                            SceneDescriptorLayout::BoundingBoxBuffer,
                            vk::DescriptorBufferInfo{.buffer = *result.boundingBoxes, .offset = 0, .range = vk::WholeSize}
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
