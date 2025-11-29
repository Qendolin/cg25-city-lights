#include "Gltf.h"

#include <fastgltf/core.hpp>
#include <fastgltf/glm_element_traits.hpp>
#include <fastgltf/math.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <stb_image.h>
#include <utility>

#include "../backend/Image.h"
#include "../entity/Light.h"
#include "../util/Logger.h"

template<typename T>
static void appendFromAccessor(std::vector<T> &dest, const fastgltf::Asset &asset, const fastgltf::Accessor &accessor) {
    size_t old_size = dest.size();
    dest.resize(old_size + accessor.count);
    fastgltf::copyFromAccessor<T>(asset, accessor, dest.data() + old_size);
}

namespace gltf {

    Scene::Scene() = default;
    Scene::~Scene() = default;

    Loader::Loader() { mParser = std::make_unique<fastgltf::Parser>(fastgltf::Extensions::KHR_lights_punctual); }
    Loader::~Loader() = default;

    Loader::Loader(Loader &&other) noexcept : mParser(std::move(other.mParser)) {}
    Loader &Loader::operator=(Loader &&other) noexcept {
        if (this == &other)
            return *this;
        mParser = std::move(other.mParser);
        return *this;
    }

    Scene Loader::load(const std::filesystem::path &path) const {
        fastgltf::Asset asset = assetFromPath(path);
        Scene scene_data;
        // Since multiple nodes/primitives can share same mesh data it's required to load in separate passes
        std::vector<PrimitiveInfo> primitive_infos;
        std::vector<std::size_t> mesh_primitive_table; // Maps mesh index to primitive info start index
        std::map<std::size_t, std::size_t> gltf_node_idx_to_anim_idx; // Maps gltf node index to animation

        loadImages(asset, scene_data);
        loadMeshData(asset, scene_data, primitive_infos, mesh_primitive_table);
        loadMaterials(asset, scene_data);
        loadAnimations(asset, scene_data, gltf_node_idx_to_anim_idx);
        loadNodes(asset, primitive_infos, mesh_primitive_table, gltf_node_idx_to_anim_idx, scene_data);

        // Sort by material for rendering efficiency
        std::ranges::sort(scene_data.sections, [](const auto &lhs, const auto &rhs) {
            return lhs.material < rhs.material;
        });

        scene_data.index_count = scene_data.index_data.size() / sizeof(uint32_t);
        scene_data.vertex_count = scene_data.vertex_position_data.size() / sizeof(glm::vec3);

        return scene_data;
    }

    void Loader::loadNodes(
            fastgltf::Asset &asset,
            const std::vector<PrimitiveInfo> &primitive_infos,
            const std::vector<std::size_t> &mesh_primitive_table,
            const std::map<std::size_t, std::size_t> &gltf_node_idx_to_anim_idx,
            Scene &scene_data
    ) {
        fastgltf::iterateSceneNodes(asset, 0, fastgltf::math::fmat4x4(), [&](fastgltf::Node &node, fastgltf::math::fmat4x4 matrix) {
            const glm::mat4 transform = glm::make_mat4x4(&matrix.col(0)[0]);
            const std::size_t gltf_node_index = static_cast<std::size_t>(&node - asset.nodes.data());
            std::uint32_t animation_index = UINT32_MAX;

            if (auto it = gltf_node_idx_to_anim_idx.find(gltf_node_index); it != gltf_node_idx_to_anim_idx.end()) {
                animation_index = static_cast<std::uint32_t>(it->second);
            }

            loadNode(asset, node, primitive_infos, mesh_primitive_table, transform, animation_index, scene_data);
        });
    }

    void Loader::loadMeshData(
            const fastgltf::Asset &asset,
            Scene &scene_data,
            std::vector<PrimitiveInfo> &primitive_infos,
            std::vector<size_t> &mesh_primitive_table
    ) {
        uint32_t vertex_offset = 0;
        uint32_t index_offset = 0;

        for (const fastgltf::Mesh &mesh: asset.meshes) {
            mesh_primitive_table.emplace_back(primitive_infos.size());
            const std::string mesh_name = std::string(mesh.name);

            gltf::Mesh &scene_mesh = scene_data.meshes.emplace_back();
            scene_mesh.name = mesh_name;

            for (const fastgltf::Primitive &primitive: mesh.primitives) {
                PrimitiveCounts primitive_counts = loadPrimitive(asset, primitive, mesh_name, scene_data, scene_mesh);

                primitive_infos.emplace_back() = {
                    .indexOffset = index_offset,
                    .indexCount = primitive_counts.index_count,
                    .vertexOffset = static_cast<int32_t>(vertex_offset),
                    .material = static_cast<uint32_t>(primitive.materialIndex.value_or(UINT32_MAX)),
                    .bounds = static_cast<uint32_t>(scene_data.bounds.size() - 1),
                };

                Logger::check(primitive.materialIndex.has_value(), std::format("Mesh {} has no material", mesh_name));

                index_offset += primitive_counts.index_count;
                vertex_offset += primitive_counts.vertex_count;
            }
        }
    }

    void Loader::loadImages(const fastgltf::Asset &asset, Scene &scene_data) {
        fastgltf::DefaultBufferDataAdapter adapter = {};
        for (const fastgltf::Image &image: asset.images) {
            Logger::check(
                    std::holds_alternative<fastgltf::sources::BufferView>(image.data),
                    "Image data source must be a buffer view"
            );
            size_t buffer_view_index = std::get<fastgltf::sources::BufferView>(image.data).bufferViewIndex;
            auto src_data = adapter(asset, buffer_view_index);
            int width, height, ch;
            auto *data = stbi_load_from_memory(
                    reinterpret_cast<stbi_uc const *>(src_data.data()), static_cast<int>(src_data.size_bytes()), &width,
                    &height, &ch, 0
            );

            int target_ch = ch == 3 ? 4 : ch; // 3 channel images are extended to 4 channels
            scene_data.images.emplace_back() = PlainImageDataU8::create(width, height, target_ch, ch, data);
        }
    }

    void Loader::loadMaterials(const fastgltf::Asset &asset, Scene &scene_data) {
        // Occlusion and roughness-metalness images may need to be merged. This cache is used for deduplication.
        std::map<std::pair<int32_t, int32_t>, int32_t> orm_cache_map;
        std::map<int32_t, int32_t> normal_cache_map;

        for (const fastgltf::Material &gltf_mat: asset.materials) {
            Material mat = initMaterialWithFactors(gltf_mat);
            mat.albedoTexture = loadMaterialAlbedoTexture(asset, gltf_mat, scene_data);
            mat.ormTexture = loadMaterialOrmTexture(asset, gltf_mat, scene_data, orm_cache_map);
            mat.normalTexture = loadMaterialNormalTexture(asset, gltf_mat, scene_data, normal_cache_map);
            scene_data.materials.push_back(mat);
        }
    }

    void Loader::loadAnimations(
            const fastgltf::Asset &asset, Scene &scene_data, std::map<std::size_t, std::size_t> &gltf_node_idx_to_anim_idx
    ) {
        for (const fastgltf::Animation &gltf_anim: asset.animations) {
            Animation animation{};

            if (gltf_anim.channels.empty()) {
                Logger::warning("Ignoring animation because it contains no channel");
                continue;
            }

            if (!gltf_anim.channels[0].nodeIndex.has_value()) {
                Logger::warning("Ignoring animation because it is not associated with any node");
                continue;
            }

            std::size_t node_index = gltf_anim.channels[0].nodeIndex.value();

            for (const fastgltf::AnimationChannel &channel: gltf_anim.channels) {
                const fastgltf::AnimationPath animPath = channel.path;

                if (animPath == fastgltf::AnimationPath::Translation)
                    loadAnimationChannel(asset, gltf_anim, channel, animation.translation_times, animation.translations);
                else if (animPath == fastgltf::AnimationPath::Rotation)
                    loadAnimationChannel(asset, gltf_anim, channel, animation.rotation_times, animation.rotations);
                else
                    Logger::debug(std::format("Ignoring unsupported weight/scale animation channel of node {}", node_index));
            }

            const std::size_t translation_count = animation.translations.size();
            const std::size_t translation_time_count = animation.translation_times.size();
            const std::size_t rotation_count = animation.rotations.size();
            const std::size_t rotation_time_count = animation.rotation_times.size();

            if (translation_time_count != translation_count) {
                Logger::warning(std::format(
                        "Ignoring translation animation of node {} because there are {} time stamps but {} values",
                        node_index, translation_time_count, translation_count
                ));
                animation.translation_times.clear();
                animation.translations.clear();
            }


            if (rotation_time_count != rotation_count) {
                Logger::warning(std::format(
                        "Ignoring rotation animation of node {} because there are {} time stamps but {} values",
                        node_index, rotation_time_count, rotation_count
                ));
                animation.rotation_times.clear();
                animation.rotations.clear();
            }

            gltf_node_idx_to_anim_idx[node_index] = scene_data.animations.size();
            scene_data.animations.push_back(animation);

            // DEBUG - TEMP
            Logger::debug(std::format("Animation of node {}:", node_index));

            if (translation_count)
                Logger::debug("<Frame Time>: <Translation>:");

            for (std::size_t i{0}; i < translation_count; ++i) {
                const glm::vec3 &translation = animation.translations[i];
                Logger::debug(std::format(
                        "{}: ({: .4f}, {: .4f}, {: .4f})", animation.translation_times[i], translation.x, translation.y,
                        translation.z
                ));
            }

            if (rotation_count)
                Logger::debug("<Frame Time>: <Rotation>:");

            for (std::size_t i{0}; i < rotation_count; ++i) {
                const glm::vec4 &rotation = animation.rotations[i];
                Logger::debug(std::format(
                        "{}: ({: .4f}, {: .4f}, {: .4f}, {: .4f})", animation.rotation_times[i], rotation.x, rotation.y,
                        rotation.z, rotation.w
                ));
            }
        }
    }

    void Loader::loadLight(
            const fastgltf::Asset &asset, Scene &scene_data, const fastgltf::Node &node, const glm::mat4 &transform, Node &scene_node
    ) {
        const fastgltf::Light &light = asset.lights[node.lightIndex.value()];
        glm::vec3 position = glm::vec3(transform[3]);
        glm::vec3 forward = glm::normalize(-glm::vec3(transform[2]));
        switch (light.type) {
            case fastgltf::LightType::Directional: {
                scene_node.directionalLight = static_cast<uint32_t>(scene_data.directionalLights.size());
                scene_data.directionalLights.emplace_back() = {
                    .elevation = glm::atan(forward.y, glm::sqrt(forward.x * forward.x + forward.z * forward.z)),
                    .azimuth = glm::atan(forward.x, forward.z),
                    .color = {light.color.x(), light.color.y(), light.color.z()},
                    .power = light.intensity / 683.0f,
                };
                break;
            }
            case fastgltf::LightType::Point: {
                scene_node.pointLight = static_cast<uint32_t>(scene_data.pointLights.size());
                scene_data.pointLights.emplace_back() = {
                    .position = position,
                    .color = {light.color.x(), light.color.y(), light.color.z()},
                    .power = light.intensity / 683.0f,
                };
                break;
            }
            case fastgltf::LightType::Spot: {
                scene_node.spotLight = static_cast<uint32_t>(scene_data.spotLights.size());
                SpotLight &scene_light = scene_data.spotLights.emplace_back() = {
                    .position = position,
                    .theta = glm::degrees(glm::atan(forward.y, glm::sqrt(forward.x * forward.x + forward.z * forward.z))),
                    .phi = glm::degrees(glm::atan(forward.x, forward.z)),
                    .color = {light.color.x(), light.color.y(), light.color.z()},
                    .power = light.intensity / 683.0f,
                };
                scene_light.outerConeAngle =
                        glm::degrees(light.outerConeAngle.value_or(glm::radians(scene_light.outerConeAngle)));
                scene_light.innerConeAngle =
                        glm::degrees(light.innerConeAngle.value_or(glm::radians(scene_light.innerConeAngle)));
                break;
            }
        }
    }

    void Loader::loadNode(
            const fastgltf::Asset &asset,
            const fastgltf::Node &node,
            const std::vector<PrimitiveInfo> &primitive_infos,
            const std::vector<size_t> &mesh_primitive_table,
            const glm::mat4 transform,
            const std::uint32_t animation_index,
            Scene &scene_data
    ) {
        size_t node_index = scene_data.nodes.size();
        gltf::Node &scene_node = scene_data.nodes.emplace_back() = {
            .name = std::string(node.name),
            .transform = transform,
            .mesh = static_cast<uint32_t>(node.meshIndex.value_or(UINT32_MAX)),
            .animation = animation_index,
        };

        if (scene_node.mesh == UINT32_MAX) {
            // Non mesh node
            if (node.lightIndex.has_value())
                loadLight(asset, scene_data, node, transform, scene_node);
            return;
        }

        const fastgltf::Mesh &mesh = asset.meshes[scene_node.mesh];

        for (std::size_t i{0}; i < mesh.primitives.size(); ++i) {
            const PrimitiveInfo &primitive_info = primitive_infos[mesh_primitive_table[scene_node.mesh] + i];

            scene_data.sections.emplace_back() = {
                .indexOffset = primitive_info.indexOffset,
                .indexCount = primitive_info.indexCount,
                .vertexOffset = primitive_info.vertexOffset,
                .node = static_cast<uint32_t>(node_index),
                .bounds = primitive_info.bounds,
                .material = primitive_info.material,
            };
        }
    }

    Loader::PrimitiveCounts Loader::loadPrimitive(
            const fastgltf::Asset &asset,
            const fastgltf::Primitive &primitive,
            std::string_view mesh_name,
            Scene &scene_data,
            gltf::Mesh &scene_mesh
    ) {
        if (primitive.type != fastgltf::PrimitiveType::Triangles)
            Logger::fatal(std::format("Mesh '{}' has primitive with non triangle type", mesh_name));

        uint32_t index_count = appendMeshPrimitiveIndices(asset, primitive, mesh_name, scene_data);

        const fastgltf::Accessor &position_accessor = getAttributeAccessor(asset, primitive, "POSITION", mesh_name);
        const fastgltf::Accessor &normal_accessor = getAttributeAccessor(asset, primitive, "NORMAL", mesh_name);
        const fastgltf::Accessor &tangent_accessor = getAttributeAccessor(asset, primitive, "TANGENT", mesh_name);
        const fastgltf::Accessor &texcoord_accessor = getAttributeAccessor(asset, primitive, "TEXCOORD_0", mesh_name);

        appendFromAccessor(scene_data.vertex_position_data, asset, position_accessor);
        appendFromAccessor(scene_data.vertex_normal_data, asset, normal_accessor);
        appendFromAccessor(scene_data.vertex_tangent_data, asset, tangent_accessor);
        appendFromAccessor(scene_data.vertex_texcoord_data, asset, texcoord_accessor);

        util::BoundingBox &bounds = scene_data.bounds.emplace_back() = {};
        for (const glm::vec3 &p: fastgltf::iterateAccessor<glm::vec3>(asset, position_accessor))
            bounds.extend(p);

        scene_mesh.bounds.extend(bounds);

        return {index_count, static_cast<uint32_t>(position_accessor.count)};
    }

    template<typename T>
    void Loader::loadAnimationChannel(
            const fastgltf::Asset &asset,
            const fastgltf::Animation &animation,
            const fastgltf::AnimationChannel &channel,
            std::vector<float> &time_stamps,
            std::vector<T> &values
    ) {
        const std::size_t node_index = channel.nodeIndex.value();
        const fastgltf::AnimationSampler &sampler = animation.samplers[channel.samplerIndex];
        const fastgltf::Accessor &time_accessor = asset.accessors[sampler.inputAccessor];
        const fastgltf::Accessor &value_accessor = asset.accessors[sampler.outputAccessor];
        const fastgltf::AnimationInterpolation interpolation = sampler.interpolation;

        if (time_accessor.type != fastgltf::AccessorType::Scalar) {
            Logger::warning(
                    std::format("Ignoring animation channel of node {} because timestamps aren't scalar values", node_index)
            );
            return;
        }

        time_stamps.clear();
        values.clear();

        appendFromAccessor(time_stamps, asset, time_accessor);

        std::vector<T> raw;
        appendFromAccessor(raw, asset, value_accessor);

        if (interpolation == fastgltf::AnimationInterpolation::CubicSpline) {
            assert(raw.size() == time_stamps.size() * 3);

            for (std::size_t k{0}; k < time_stamps.size(); ++k)
                values.push_back(raw[k * 3 + 1]);
        } else {
            assert(raw.size() == time_stamps.size());

            for (std::size_t k{0}; k < time_stamps.size(); ++k)
                values.push_back(raw[k]);
        }
    }

    Material Loader::initMaterialWithFactors(const fastgltf::Material &gltf_mat) {
        Material mat{};

        mat.albedoFactor = glm::vec4(
                gltf_mat.pbrData.baseColorFactor.x(), gltf_mat.pbrData.baseColorFactor.y(),
                gltf_mat.pbrData.baseColorFactor.z(), gltf_mat.pbrData.baseColorFactor.w()
        );
        mat.metalnessFactor = gltf_mat.pbrData.metallicFactor;
        mat.roughnessFactor = gltf_mat.pbrData.roughnessFactor;
        if (gltf_mat.normalTexture.has_value())
            mat.normalFactor = gltf_mat.normalTexture->scale;

        return mat;
    }

    int32_t Loader::loadMaterialAlbedoTexture(const fastgltf::Asset &asset, const fastgltf::Material &gltf_mat, Scene &scene_data) {
        int32_t texture_index = -1;

        if (gltf_mat.pbrData.baseColorTexture.has_value()) {
            texture_index = static_cast<int32_t>(
                    asset.textures[gltf_mat.pbrData.baseColorTexture.value().textureIndex].imageIndex.value()
            );
            PlainImageDataU8 &image = scene_data.images[texture_index];
            if (image.format == vk::Format::eUndefined) // claim image in this format
                image.format = vk::Format::eR8G8B8A8Srgb;
            Logger::check(image.format == vk::Format::eR8G8B8A8Srgb, "Format of albedo texture must be R8G8B8A8_SRGB");
        }

        return texture_index;
    }

    int32_t Loader::loadMaterialOrmTexture(
            const fastgltf::Asset &asset,
            const fastgltf::Material &gltf_mat,
            Scene &scene_data,
            std::map<std::pair<int32_t, int32_t>, int32_t> &orm_cache_map
    ) {
        auto getImageIndex = [&](const fastgltf::TextureInfo &texInfo) -> int32_t {
            const size_t texIndex = texInfo.textureIndex;
            const size_t imageIndex = asset.textures[texIndex].imageIndex.value();
            return static_cast<int32_t>(imageIndex);
        };

        if (!gltf_mat.occlusionTexture.has_value() && !gltf_mat.pbrData.metallicRoughnessTexture.has_value())
            return -1;

        std::pair<int32_t, int32_t> orm_cache_key = {-1, -1};
        PlainImageDataU8 *o_image = nullptr;
        PlainImageDataU8 *rm_image = nullptr;

        if (gltf_mat.occlusionTexture.has_value()) {
            int32_t index = getImageIndex(gltf_mat.occlusionTexture.value());
            o_image = &scene_data.images[index];
            orm_cache_key.first = index;
        }

        if (gltf_mat.pbrData.metallicRoughnessTexture.has_value()) {
            int32_t index = getImageIndex(gltf_mat.pbrData.metallicRoughnessTexture.value());
            rm_image = &scene_data.images[index];
            orm_cache_key.second = index;
        }

        // ORM texture merging

        if (o_image && rm_image && o_image == rm_image) {
            int32_t texture_index = getImageIndex(gltf_mat.occlusionTexture.value());
            PlainImageDataU8 &image = scene_data.images[texture_index];

            if (image.format == vk::Format::eUndefined)
                image.format = vk::Format::eR8G8B8A8Unorm;

            Logger::check(image.format == vk::Format::eR8G8B8A8Unorm, "Format of orm texture must be R8G8B8A8_UNORM");
            return texture_index;
        }

        if (o_image && rm_image && (rm_image->width != o_image->width || rm_image->height != o_image->height))
            Logger::fatal("Occlusion and roughness-metalness texture sizes don't match");

        if (auto it = orm_cache_map.find(orm_cache_key); it != orm_cache_map.end())
            return it->second;

        PlainImageDataU8 *o_or_rm_image = o_image ? o_image : rm_image;
        int32_t texture_index = static_cast<int32_t>(scene_data.images.size());

        if (!o_or_rm_image)
            return -1; // Keep analyzer happy

        PlainImageDataU8 orm_image =
                PlainImageDataU8::create(vk::Format::eR8G8B8A8Unorm, o_or_rm_image->width, o_or_rm_image->height);

        if (o_image)
            o_image->copyChannels(orm_image, {0});

        if (rm_image)
            rm_image->copyChannels(orm_image, {1, 2});
        else
            orm_image.fill({1, 2}, {0xff, 0xff});

        orm_cache_map[orm_cache_key] = texture_index;
        scene_data.images.push_back(std::move(orm_image));

        return texture_index;
    }

    int32_t Loader::loadMaterialNormalTexture(
            const fastgltf::Asset &asset,
            const fastgltf::Material &gltf_mat,
            Scene &scene_data,
            std::map<int32_t, int32_t> &normal_cache_map
    ) {
        int32_t texture_index = -1;

        if (gltf_mat.normalTexture.has_value()) {
            int32_t index = asset.textures[gltf_mat.normalTexture.value().textureIndex].imageIndex.value();
            if (normal_cache_map.contains(index)) {
                texture_index = normal_cache_map.at(index);
            } else {
                const PlainImageDataU8 &src_image = scene_data.images[index];
                PlainImageDataU8 normal = PlainImageDataU8::create(vk::Format::eR8G8Unorm, src_image.width, src_image.height);
                src_image.copyChannels(normal, {0, 1});
                texture_index = static_cast<int32_t>(scene_data.images.size());
                scene_data.images.emplace_back(std::move(normal));
                normal_cache_map[index] = texture_index;
            }
        }

        return texture_index;
    }

    fastgltf::Asset Loader::assetFromPath(const std::filesystem::path &path) const {
        auto data = fastgltf::GltfDataBuffer::FromPath(path);

        if (data.error() != fastgltf::Error::None)
            Logger::fatal(std::format("Failed to load GLTF: {}", fastgltf::getErrorName(data.error())));

        auto asset = mParser->loadGltf(data.get(), path.parent_path(), fastgltf::Options::None);

        if (asset.error() != fastgltf::Error::None)
            Logger::fatal(std::format("Failed to load GLTF: {}", fastgltf::getErrorName(asset.error())));

        return *std::move(asset);
    }

    const fastgltf::Accessor &Loader::getAttributeAccessor(
            const fastgltf::Asset &asset,
            const fastgltf::Primitive &primitive,
            std::string_view attribute_name,
            std::string_view mesh_name
    ) {
        const auto it = primitive.findAttribute(attribute_name);

        if (it == primitive.attributes.cend())
            Logger::fatal(std::format("Mesh '{}' has primitive that's missing a '{}' attribute", mesh_name, attribute_name));

        return asset.accessors[it->accessorIndex];
    }

    uint32_t Loader::appendMeshPrimitiveIndices(
            const fastgltf::Asset &asset, const fastgltf::Primitive &primitive, std::string_view mesh_name, gltf::Scene &scene_data
    ) {
        if (!primitive.indicesAccessor.has_value())
            Logger::fatal(std::format("Mesh '{}' has primitive without index accessor", mesh_name));

        const fastgltf::Accessor &index_accessor = asset.accessors[primitive.indicesAccessor.value()];
        const fastgltf::ComponentType type = index_accessor.componentType;

        if (type != fastgltf::ComponentType::UnsignedInt && type != fastgltf::ComponentType::UnsignedShort) {
            Logger::warning(std::format("Mesh '{}' has indices which aren't unsigned shorts or unsigned ints", mesh_name));
            return 0;
        }

        if (index_accessor.componentType == fastgltf::ComponentType::UnsignedInt)
            appendFromAccessor(scene_data.index_data, asset, index_accessor);
        else if (index_accessor.componentType == fastgltf::ComponentType::UnsignedShort) {
            const size_t old_size = scene_data.index_data.size();

            scene_data.index_data.resize(old_size + index_accessor.count);

            fastgltf::iterateAccessorWithIndex<std::uint32_t>(asset, index_accessor, [&](std::uint32_t index, size_t i) {
                scene_data.index_data[old_size + i] = index;
            });
        }

        scene_data.index_count += index_accessor.count;
        return index_accessor.count;
    }

} // namespace gltf
