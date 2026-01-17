#include "Loader.h"

#include <algorithm>
#include <array>
#include <utility>

#include "../backend/StagingBuffer.h"
#include "../debug/Annotation.h"
#include "../entity/Light.h"
#include "../util/Logger.h"
#include "Gltf.h"
#include "gpu_types.h"

namespace scene {
    Loader::Loader(
            const vma::Allocator &allocator,
            const vk::Device &device,
            const vk::PhysicalDevice &physical_device,
            const DeviceQueue &transferQueue,
            const DeviceQueue &graphicsQueue
    )
        : mAllocator(allocator),
          mDevice(device),
          mPhysicalDevice(physical_device),
          mTransferQueue(transferQueue),
          mGraphicsQueue(graphicsQueue) {}

    Scene Loader::load(const std::filesystem::path &path) const {
        gltf::Loader gltf_loader;
        gltf::Scene gltf_scene = gltf_loader.load(path);
        CpuData cpu_data = createCpuData(gltf_scene);
        GpuData gpu_data = createGpuData(gltf_scene);

        return {std::move(cpu_data), std::move(gpu_data)};
    }

    CpuData Loader::createCpuData(const gltf::Scene &scene_data) const {
        CpuData cpu_data{};

        const std::size_t node_count = scene_data.nodes.size();
        const std::size_t animated_node_count = std::ranges::count_if(scene_data.nodes, [](const gltf::Node &node) {
            return node.mesh != UINT32_MAX && node.animation != UINT32_MAX;
        });

        cpu_data.instances.reserve(node_count);
        cpu_data.instance_animations.reserve(animated_node_count);

        // animated mesh instances get sorted to the end
        std::vector<Instance> anim_inst_to_insert_last;
        anim_inst_to_insert_last.reserve(animated_node_count);

        for (const gltf::Node &node: scene_data.nodes) {
            const bool hasMesh = (node.mesh != UINT32_MAX);
            const bool hasAnimation = (node.animation != UINT32_MAX);
            util::BoundingBox bounds{};

            if (hasMesh)
                bounds = scene_data.meshes[node.mesh].bounds;

            Instance instance{node.name, node.transform, bounds};

            if (hasAnimation && !hasMesh) {
                const gltf::Animation &anim_data = scene_data.animations[node.animation];
                cpu_data.non_mesh_instance_animation_map[instance.name] =
                        std::make_pair(cpu_data.instances.size(), cpu_data.non_mesh_instance_animations.size());
                cpu_data.non_mesh_instance_animations.push_back(createInstanceAnimation(anim_data));
                cpu_data.instances.push_back(instance);
                continue;
            }

            // Add nodes with meshes and animations lastly, to enable easier transform updates on the GPU
            if (hasAnimation) {
                const gltf::Animation &anim_data = scene_data.animations[node.animation];
                cpu_data.instance_animations.push_back(createInstanceAnimation(anim_data));
                anim_inst_to_insert_last.push_back(instance);
            } else
                cpu_data.instances.push_back(instance);
        }

        cpu_data.instances.insert(cpu_data.instances.end(), anim_inst_to_insert_last.begin(), anim_inst_to_insert_last.end());

        cpu_data.lights = createLights(scene_data);
        createCpuDataInitNamedLightAnimations(scene_data, cpu_data);

        return cpu_data;
    }

    InstanceAnimation Loader::createInstanceAnimation(const gltf::Animation &animation_data) {
        std::vector<glm::quat> quats(animation_data.rotations.size());

        std::ranges::transform(animation_data.rotations, quats.begin(), [](const glm::vec4 &v) {
            return glm::quat(v.w, v.x, v.y, v.z);
        });

        return InstanceAnimation{
            animation_data.translation_timestamps,
            animation_data.rotation_timestamps,
            animation_data.scale_timestamps,
            animation_data.translations,
            std::move(quats),
            animation_data.scales
        };
    }

    // Note Felix 16.01.26: See note in header
    void Loader::createCpuDataInitNamedLightAnimations(const gltf::Scene &scene_data, scene::CpuData &cpuData) const {
        std::size_t light_index{0};

        for (const PointLight &light: scene_data.pointLights) {
            // We assume that named animatioins are named after their associated nodes/instances
            if (cpuData.non_mesh_instance_animation_map.contains(light.node_name))
                cpuData.named_light_animations.push_back({light.node_name, light_index});

            ++light_index;
        }

        for (const SpotLight &light: scene_data.spotLights) {
            if (cpuData.non_mesh_instance_animation_map.contains(light.node_name))
                cpuData.named_light_animations.push_back({light.node_name, light_index});

            ++light_index;
        }

        Logger::debug(std::format("Named light animations: {}", cpuData.named_light_animations.size()));
    }

    GpuData Loader::createGpuData(const gltf::Scene &scene_data) const {
        vk::CommandPoolCreateInfo graphics_cmd_pool_create_info{};
        graphics_cmd_pool_create_info.setFlags(vk::CommandPoolCreateFlagBits::eTransient).setQueueFamilyIndex(mGraphicsQueue);
        vk::UniqueCommandPool graphics_cmd_pool = mDevice.createCommandPoolUnique(graphics_cmd_pool_create_info);

        vk::CommandPoolCreateInfo transfer_cmd_pool_create_info{};
        transfer_cmd_pool_create_info.setFlags(vk::CommandPoolCreateFlagBits::eTransient).setQueueFamilyIndex(mTransferQueue);
        vk::UniqueCommandPool transfer_cmd_pool = mDevice.createCommandPoolUnique(transfer_cmd_pool_create_info);

        vk::CommandBufferAllocateInfo graphics_cmds_allocate_info{};
        graphics_cmds_allocate_info.setCommandPool(*graphics_cmd_pool).setCommandBufferCount(1);
        vk::CommandBuffer graphics_cmds = mDevice.allocateCommandBuffers(graphics_cmds_allocate_info).at(0);

        GpuData gpu_data;
        StagingBuffer staging = {mAllocator, mDevice, *transfer_cmd_pool};

        createGpuDataInitDescriptorPool(gpu_data);
        createGpuDataInitDescriptorSet(gpu_data);
        createGpuDataInitSampler(gpu_data);
        const auto image_indices = createGpuDataInitImages(scene_data, graphics_cmds, staging, gpu_data);

        vk::UniqueSemaphore image_transfer_semaphore = mDevice.createSemaphoreUnique({});
        vk::UniqueFence fence = mDevice.createFenceUnique({});
        staging.submit(mTransferQueue, vk::SubmitInfo().setSignalSemaphores(*image_transfer_semaphore));
        vk::PipelineStageFlags semaphore_stage_mask = vk::PipelineStageFlagBits::eTopOfPipe;
        vk::SubmitInfo submitInfo{};
        submitInfo.setWaitSemaphores(*image_transfer_semaphore)
                .setCommandBuffers(graphics_cmds)
                .setWaitDstStageMask(semaphore_stage_mask);
        mGraphicsQueue.queue.submit(submitInfo, *fence);

        createGpuDataInitVertices(scene_data, staging, gpu_data);
        const auto node_instance_map = createGpuDataInitInstances(scene_data, staging, gpu_data);
        createGpuDataInitSections(scene_data, staging, node_instance_map, gpu_data);
        createGpuDataInitMaterials(scene_data, staging, image_indices, gpu_data);
        createGpuDataInitLights(scene_data, staging, gpu_data);
        createGpuDataUpdateDescriptorSet(gpu_data);

        staging.submit(mTransferQueue);

        const vk::Result waitRes = mDevice.waitForFences(*fence, vk::True, UINT64_MAX);

        if (waitRes != vk::Result::eSuccess)
            Logger::fatal(std::format("waitForFences failed: {}", vk::to_string(waitRes)));

        return gpu_data;
    }

    void Loader::createGpuDataInitDescriptorPool(GpuData &gpu_data) const {
        gpu_data.sceneDescriptorLayout = SceneDescriptorLayout(mDevice);

        std::array pool_sizes = {
            vk::DescriptorPoolSize{vk::DescriptorType::eUniformBuffer, UNIFORM_BUFFER_POOL_SIZE},
            vk::DescriptorPoolSize{vk::DescriptorType::eCombinedImageSampler, COMBINED_IMAGE_SAMPLER_POOL_SIZE},
            vk::DescriptorPoolSize{vk::DescriptorType::eStorageBuffer, STORAGE_BUFFER_POOL_SIZE},
        };

        gpu_data.sceneDescriptorPool =
                mDevice.createDescriptorPoolUnique(vk::DescriptorPoolCreateInfo{.maxSets = 1}.setPoolSizes(pool_sizes));
        util::setDebugName(mDevice, *gpu_data.sceneDescriptorPool, "descriptor_pool");
    }

    void Loader::createGpuDataInitDescriptorSet(GpuData &gpu_data) const {
        vk::DescriptorSetLayout vk_layout = gpu_data.sceneDescriptorLayout;

        gpu_data.sceneDescriptor = DescriptorSet(mDevice.allocateDescriptorSets({
            .descriptorPool = *gpu_data.sceneDescriptorPool,
            .descriptorSetCount = 1,
            .pSetLayouts = &vk_layout,
        })[0]);

        util::setDebugName(mDevice, vk::DescriptorSet(gpu_data.sceneDescriptor), "descriptor_set");
    }

    void Loader::createGpuDataInitSampler(GpuData &gpu_data) const {
        float max_anisotropy = mPhysicalDevice.getProperties().limits.maxSamplerAnisotropy;

        gpu_data.sampler = mDevice.createSamplerUnique(
                {.magFilter = vk::Filter::eLinear,
                 .minFilter = vk::Filter::eLinear,
                 .mipmapMode = vk::SamplerMipmapMode::eLinear,
                 .anisotropyEnable = true,
                 .maxAnisotropy = max_anisotropy,
                 .maxLod = vk::LodClampNone,
                 .borderColor = vk::BorderColor::eFloatOpaqueBlack}
        );

        util::setDebugName(mDevice, *gpu_data.sampler, "sampler");
    }

    std::vector<uint32_t> Loader::createGpuDataInitImages(
            const gltf::Scene &scene_data, const vk::CommandBuffer &graphics_cmds, StagingBuffer &staging, GpuData &gpu_data
    ) const {
        std::vector<uint32_t> image_indices;
        gpu_data.images.reserve(scene_data.images.size());
        gpu_data.views.reserve(scene_data.images.size());

        graphics_cmds.begin({.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit});

        for (const auto &image_data: scene_data.images) {
            // image isn't used by any material
            if (image_data.format == vk::Format::eUndefined) {
                image_indices.emplace_back(-1); // Note Felix: hard to read black-magic, I won't touch it
                continue;
            }
            uint32_t index = static_cast<uint32_t>(gpu_data.images.size());
            image_indices.emplace_back(index);

            ImageCreateInfo create_info = {
                .format = image_data.format,
                .aspects = vk::ImageAspectFlagBits::eColor,
                .width = image_data.width,
                .height = image_data.height,
                .levels = UINT32_MAX,
                .usage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferSrc |
                         vk::ImageUsageFlagBits::eTransferDst,
            };
            Image &image = gpu_data.images.emplace_back();
            image = Image::create(staging.allocator(), create_info);
            util::setDebugName(mDevice, *image.image, std::format("image_{}", index));

            vk::Buffer staged_buffer = staging.stage(image_data.pixels);
            image.load(staging.commands(), 0, {}, staged_buffer);
            image.transfer(staging.commands(), graphics_cmds, mTransferQueue, mGraphicsQueue);
            image.generateMipmaps(graphics_cmds);
            image.barrier(graphics_cmds, ImageResourceAccess::FragmentShaderReadOptimal);

            ImageView &view = gpu_data.views.emplace_back();
            view = ImageView::create(mDevice, image);
            util::setDebugName(mDevice, *view.view, std::format("image_view_{}", index));

            mDevice.updateDescriptorSets(
                    {gpu_data.sceneDescriptor.write(
                            SceneDescriptorLayout::ImageSamplers,
                            vk::DescriptorImageInfo{
                                .sampler = *gpu_data.sampler, .imageView = view, .imageLayout = vk::ImageLayout::eReadOnlyOptimal
                            },
                            index
                    )},
                    {}
            );
        }

        graphics_cmds.end();

        return image_indices;
    }

    void Loader::createGpuDataInitVertices(const gltf::Scene &scene_data, StagingBuffer &staging, GpuData &gpu_data) const {
        uploadBufferWithDebugName(
                staging, scene_data.vertex_position_data, vk::BufferUsageFlagBits::eVertexBuffer, "vertex_positions",
                gpu_data.positions, gpu_data.positionsAlloc
        );
        uploadBufferWithDebugName(
                staging, scene_data.vertex_normal_data, vk::BufferUsageFlagBits::eVertexBuffer, "vertex_normals",
                gpu_data.normals, gpu_data.normalsAlloc
        );
        uploadBufferWithDebugName(
                staging, scene_data.vertex_tangent_data, vk::BufferUsageFlagBits::eVertexBuffer, "vertex_tangents",
                gpu_data.tangents, gpu_data.tangentsAlloc
        );
        uploadBufferWithDebugName(
                staging, scene_data.vertex_texcoord_data, vk::BufferUsageFlagBits::eVertexBuffer, "vertex_texcoords",
                gpu_data.texcoords, gpu_data.texcoordsAlloc
        );
        uploadBufferWithDebugName(
                staging, scene_data.index_data, vk::BufferUsageFlagBits::eIndexBuffer, "vertex_indices",
                gpu_data.indices, gpu_data.indicesAlloc
        );
    }

    std::vector<glm::uint> Loader::createGpuDataInitInstances(
            const gltf::Scene &scene_data, StagingBuffer &staging, GpuData &gpu_data
    ) const {
        const std::vector<gltf::Node> &nodes = scene_data.nodes;

        const std::size_t instance_count = std::ranges::count_if(nodes, [](const gltf::Node &node) {
            return node.mesh != UINT32_MAX;
        });

        const std::size_t animated_instance_count = std::ranges::count_if(nodes, [](const gltf::Node &node) {
            return node.mesh != UINT32_MAX && node.animation != UINT32_MAX;
        });

        const std::size_t static_instance_count = instance_count - animated_instance_count;

        std::vector<InstanceBlock> instance_blocks;
        instance_blocks.reserve(instance_count);

        std::vector<InstanceBlock> anim_inst_blocks_to_insert_last;
        anim_inst_blocks_to_insert_last.reserve(animated_instance_count);

        // maps *node* index to *instance* index
        std::vector<glm::uint> node_instance_map(nodes.size(), UINT32_MAX);

        for (std::size_t i = 0; i < nodes.size(); ++i) {
            const auto &node = nodes[i];

            if (node.mesh == UINT32_MAX) {
                node_instance_map[i] = UINT32_MAX;
                continue;
            }

            InstanceBlock instance_block{node.transform};

            if (node.animation == UINT32_MAX) {
                node_instance_map[i] = static_cast<glm::uint>(instance_blocks.size());
                instance_blocks.push_back(instance_block);
            } else {
                std::size_t instance_index = static_instance_count + anim_inst_blocks_to_insert_last.size();
                node_instance_map[i] = static_cast<glm::uint>(instance_index);
                anim_inst_blocks_to_insert_last.push_back(instance_block);
            }
        }

        instance_blocks.insert(
                instance_blocks.end(), anim_inst_blocks_to_insert_last.begin(), anim_inst_blocks_to_insert_last.end()
        );

        auto [raw_instance_buffer,
              instance_alloc] = staging.upload(instance_blocks, vk::BufferUsageFlagBits::eStorageBuffer);
        gpu_data.instances = Buffer(
                std::move(raw_instance_buffer), std::move(instance_alloc), instance_blocks.size() * sizeof(InstanceBlock)
        );
        util::setDebugName(mDevice, static_cast<vk::Buffer>(gpu_data.instances), "instances");

        return node_instance_map;
    }

    void Loader::createGpuDataInitSections(
            const gltf::Scene &scene_data, StagingBuffer &staging, const std::vector<glm::uint> &node_instance_map, GpuData &gpu_data
    ) const {
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

        uploadBufferWithDebugName(
                staging, draw_commands, vk::BufferUsageFlagBits::eIndirectBuffer | vk::BufferUsageFlagBits::eStorageBuffer,
                "draw_commands", gpu_data.drawCommands, gpu_data.drawCommandsAlloc
        );

        gpu_data.drawCommandCount = static_cast<uint32_t>(draw_commands.size());

        uploadBufferWithDebugName(
                staging, section_blocks, vk::BufferUsageFlagBits::eStorageBuffer, "sections", gpu_data.sections,
                gpu_data.sectionsAlloc
        );

        uploadBufferWithDebugName(
                staging, bounding_box_blocks, vk::BufferUsageFlagBits::eStorageBuffer, "bounding_boxes",
                gpu_data.boundingBoxes, gpu_data.boundingBoxesAlloc
        );
    }

    void Loader::createGpuDataInitMaterials(
            const gltf::Scene &scene_data, StagingBuffer &staging, const std::vector<uint32_t> &image_indices, GpuData &gpu_data
    ) const {
        std::vector<MaterialBlock> material_blocks;
        material_blocks.reserve(scene_data.materials.size());
        for (const auto &material: scene_data.materials) {
            uint32_t albedo_texture_index = material.albedoTexture == -1 ? 0xffff : image_indices[material.albedoTexture];
            uint32_t normal_texture_index = material.normalTexture == -1 ? 0xffff : image_indices[material.normalTexture];
            uint32_t orm_texture_index = material.ormTexture == -1 ? 0xffff : image_indices[material.ormTexture];
            material_blocks.emplace_back() = {
                .albedoFactors = material.albedoFactor,
                .rmneFactors =
                        glm::vec4{
                            material.roughnessFactor, material.metalnessFactor, material.normalFactor, material.emissiveStrength
                        },
                .packedImageIndices0 = albedo_texture_index & 0xffff | (normal_texture_index & 0xffff) << 16,
                .packedImageIndices1 = orm_texture_index & 0xffff,
            };
        }

        uploadBufferWithDebugName(
                staging, material_blocks, vk::BufferUsageFlagBits::eStorageBuffer, "materials", gpu_data.materials,
                gpu_data.materialsAlloc
        );
    }

    std::vector<UberLightBlock> Loader::createLights(const gltf::Scene &scene_data) const {
        // Note by Felix: Many magic numbers. I won't touch them because I do not understand them

        float light_range_epsilon = 1.0f / 128.0f;
        std::vector<UberLightBlock> uber_light_blocks;
        uber_light_blocks.reserve(scene_data.pointLights.size() + scene_data.spotLights.size() + DYNAMIC_LIGHTS_RESERVATION);

        for (const auto &light: scene_data.pointLights) {
            uber_light_blocks.emplace_back() = {
                .position = light.position,
                .range = 0,
                .radiance = light.radiance(),
                .pointSize = 0.05f,
            };
            uber_light_blocks.back().updateRange(light_range_epsilon);
        }

        for (const auto &light: scene_data.spotLights) {
            float angle_scale = 1.0f / std::max(
                                               0.001f, glm::cos(glm::radians(light.innerConeAngle)) -
                                                               glm::cos(glm::radians(light.outerConeAngle))
                                       );
            float angle_offset = -glm::cos(glm::radians(light.outerConeAngle)) * angle_scale;
            uber_light_blocks.emplace_back() = {
                .position = light.position,
                .range = 0,
                .radiance = light.radiance(),
                .coneAngleScale = angle_scale,
                .direction = util::octahedronEncode(light.direction()),
                .pointSize = 0.05f,
                .coneAngleOffset = angle_offset,
            };
            uber_light_blocks.back().updateRange(light_range_epsilon);
        }

        uber_light_blocks.resize(uber_light_blocks.capacity());

        return uber_light_blocks;
    }

    void Loader::createGpuDataInitLights(const gltf::Scene &scene_data, StagingBuffer &staging, GpuData &gpu_data) const {
        auto uber_light_blocks = createLights(scene_data);

        vma::UniqueBuffer out_buffer;
        vma::UniqueAllocation out_alloc;
        uploadBufferWithDebugName(
                staging, uber_light_blocks, vk::BufferUsageFlagBits::eStorageBuffer, "uber_lights", out_buffer, out_alloc
        );
        gpu_data.uberLights =
                Buffer(std::move(out_buffer), std::move(out_alloc), uber_light_blocks.size() * sizeof(UberLightBlock));
    }

    void Loader::createGpuDataUpdateDescriptorSet(GpuData &gpu_data) const {
        mDevice.updateDescriptorSets(
                {
                    gpu_data.sceneDescriptor.write(
                            SceneDescriptorLayout::SectionBuffer,
                            vk::DescriptorBufferInfo{.buffer = *gpu_data.sections, .offset = 0, .range = vk::WholeSize}
                    ),
                    gpu_data.sceneDescriptor.write(
                            SceneDescriptorLayout::InstanceBuffer,
                            vk::DescriptorBufferInfo{.buffer = gpu_data.instances, .offset = 0, .range = vk::WholeSize}
                    ),
                    gpu_data.sceneDescriptor.write(
                            SceneDescriptorLayout::MaterialBuffer,
                            vk::DescriptorBufferInfo{.buffer = *gpu_data.materials, .offset = 0, .range = vk::WholeSize}
                    ),
                    gpu_data.sceneDescriptor.write(
                            SceneDescriptorLayout::UberLightBuffer,
                            vk::DescriptorBufferInfo{.buffer = gpu_data.uberLights, .offset = 0, .range = vk::WholeSize}
                    ),
                    gpu_data.sceneDescriptor.write(
                            SceneDescriptorLayout::BoundingBoxBuffer,
                            vk::DescriptorBufferInfo{.buffer = *gpu_data.boundingBoxes, .offset = 0, .range = vk::WholeSize}
                    ),
                },
                {}
        );
    }

    template<typename T>
    void Loader::uploadBufferWithDebugName(
            StagingBuffer &staging,
            T &&src,
            vk::BufferUsageFlags usage,
            const std::string &debugName,
            vma::UniqueBuffer &outBuffer,
            vma::UniqueAllocation &outAlloc
    ) const {
        std::tie(outBuffer, outAlloc) = staging.upload(std::forward<T>(src), usage);
        util::setDebugName(mDevice, *outBuffer, DEBUG_NAME_PREFIX + debugName);
    }

} // namespace scene
