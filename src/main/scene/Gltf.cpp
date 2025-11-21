#include "Gltf.h"

#include <fastgltf/core.hpp>
#include <fastgltf/glm_element_traits.hpp>
#include <fastgltf/math.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <map>
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

    void Loader::loadMeshData(
            const fastgltf::Asset &asset,
            Scene &scene_data,
            std::vector<PrimitiveInfo> &primitive_infos,
            std::vector<size_t> &mesh_primitive_table
    ) {
        uint32_t vertex_offset = 0;
        uint32_t index_offset = 0;
        for (const auto &mesh: asset.meshes) {
            mesh_primitive_table.emplace_back(primitive_infos.size());

            auto &scene_mesh = scene_data.meshes.emplace_back() = {
                .name = std::string(mesh.name),
            };

            for (const auto &primitive: mesh.primitives) {
                if (primitive.type != fastgltf::PrimitiveType::Triangles) {
                    Logger::fatal(std::format("Mesh '{}' has primitive with non triangle type", mesh.name));
                }

                if (!primitive.indicesAccessor.has_value()) {
                    Logger::fatal(std::format("Mesh '{}' has primitive without index accessor", mesh.name));
                }

                auto position_attr = primitive.findAttribute("POSITION");
                if (position_attr == primitive.attributes.cend()) {
                    Logger::fatal(std::format("Mesh '{}' has primitive that's missing a POSITION attribute", mesh.name));
                }
                const auto &position_accessor = asset.accessors[position_attr->accessorIndex];

                auto normal_attr = primitive.findAttribute("NORMAL");
                if (normal_attr == primitive.attributes.cend()) {
                    Logger::fatal(std::format("Mesh '{}' has primitive that's missing a NORMAL attribute", mesh.name));
                }
                const auto &normal_accessor = asset.accessors[normal_attr->accessorIndex];

                auto tangent_attr = primitive.findAttribute("TANGENT");
                if (tangent_attr == primitive.attributes.cend()) {
                    Logger::fatal(std::format("Mesh '{}' has primitive that's missing a TANGENT attribute", mesh.name));
                }
                const auto &tangent_accessor = asset.accessors[tangent_attr->accessorIndex];

                auto texcoord_attr = primitive.findAttribute("TEXCOORD_0");
                if (texcoord_attr == primitive.attributes.cend()) {
                    Logger::fatal(std::format("Mesh '{}' has primitive that's missing a TEXCOORD_0 attribute", mesh.name));
                }
                const auto &texcoord_accessor = asset.accessors[texcoord_attr->accessorIndex];

                if (!primitive.indicesAccessor.has_value()) {
                    Logger::fatal(std::format("Mesh '{}' has primitive that's missing an index accessor", mesh.name));
                }
                const auto &index_accessor = asset.accessors[primitive.indicesAccessor.value()];

                appendFromAccessor(scene_data.vertex_position_data, asset, position_accessor);
                appendFromAccessor(scene_data.vertex_normal_data, asset, normal_accessor);
                appendFromAccessor(scene_data.vertex_tangent_data, asset, tangent_accessor);
                appendFromAccessor(scene_data.vertex_texcoord_data, asset, texcoord_accessor);
                if (index_accessor.componentType == fastgltf::ComponentType::UnsignedInt) {
                    appendFromAccessor(scene_data.index_data, asset, index_accessor);
                } else if (index_accessor.componentType == fastgltf::ComponentType::UnsignedShort) {
                    size_t old_size = scene_data.index_data.size();
                    scene_data.index_data.resize(old_size + index_accessor.count);
                    fastgltf::iterateAccessorWithIndex<std::uint32_t>(
                            asset, index_accessor,
                            [&](std::uint16_t index, size_t i) { scene_data.index_data[old_size + i] = index; }
                    );
                } else {
                    Logger::check(
                            false,
                            std::format("Mesh '{}' has indices which aren't unsigned shorts or unsigned ints", mesh.name)
                    );
                }
                scene_data.index_count += index_accessor.count;

                util::BoundingBox &bounds = scene_data.bounds.emplace_back() = {};
                for (const glm::vec3 &p: fastgltf::iterateAccessor<glm::vec3>(asset, position_accessor)) {
                    bounds.extend(p);
                }
                scene_mesh.bounds.extend(bounds);

                primitive_infos.emplace_back() = {
                    .indexOffset = index_offset,
                    .indexCount = static_cast<uint32_t>(index_accessor.count),
                    .vertexOffset = static_cast<int32_t>(vertex_offset),
                    .material = static_cast<uint32_t>(primitive.materialIndex.value_or(UINT32_MAX)),
                    .bounds = static_cast<uint32_t>(scene_data.bounds.size() - 1),
                };
                index_offset += static_cast<uint32_t>(index_accessor.count);
                vertex_offset += static_cast<uint32_t>(position_accessor.count);
            }
        }
    }

    void Loader::loadImages(const fastgltf::Asset &asset, Scene &scene_data) {
        fastgltf::DefaultBufferDataAdapter adapter = {};
        for (const auto &image: asset.images) {
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

        // FIXME: The image code is way too complex and scuffed

        for (const auto &gltf_mat: asset.materials) {
            Material &mat = scene_data.materials.emplace_back();
            mat.albedoFactor = glm::vec4(
                    gltf_mat.pbrData.baseColorFactor.x(), gltf_mat.pbrData.baseColorFactor.y(),
                    gltf_mat.pbrData.baseColorFactor.z(), gltf_mat.pbrData.baseColorFactor.w()
            );
            mat.metalnessFactor = gltf_mat.pbrData.metallicFactor;
            mat.roughnessFactor = gltf_mat.pbrData.roughnessFactor;
            mat.normalFactor = 1.0;
            if (gltf_mat.normalTexture.has_value())
                mat.normalFactor = gltf_mat.normalTexture->scale;

            if (gltf_mat.pbrData.baseColorTexture.has_value()) {
                mat.albedoTexture = static_cast<int32_t>(
                        asset.textures[gltf_mat.pbrData.baseColorTexture.value().textureIndex].imageIndex.value()
                );
                auto &image = scene_data.images[mat.albedoTexture];
                if (image.format == vk::Format::eUndefined) // claim image in this format
                    image.format = vk::Format::eR8G8B8A8Srgb;
                Logger::check(image.format == vk::Format::eR8G8B8A8Srgb, "Format of albedo texture must be R8G8B8A8_SRGB");
            }

            std::pair orm_cache_key = {-1, -1};
            PlainImageDataU8 *o_image = nullptr;
            PlainImageDataU8 *rm_image = nullptr;
            if (gltf_mat.occlusionTexture.has_value()) {
                auto index = static_cast<int32_t>(
                        asset.textures[gltf_mat.occlusionTexture.value().textureIndex].imageIndex.value()
                );
                o_image = &scene_data.images[index];
                orm_cache_key.first = index;
            }

            if (gltf_mat.pbrData.metallicRoughnessTexture.has_value()) {
                auto index = static_cast<int32_t>(
                        asset.textures[gltf_mat.pbrData.metallicRoughnessTexture.value().textureIndex].imageIndex.value()
                );
                rm_image = &scene_data.images[index];
                orm_cache_key.second = index;
            }

            // ORM texture merging logic
            if (o_image != nullptr && rm_image != nullptr && o_image == rm_image) {
                mat.ormTexture = static_cast<int32_t>(
                        asset.textures[gltf_mat.occlusionTexture.value().textureIndex].imageIndex.value()
                );
                auto &image = scene_data.images[mat.ormTexture];
                if (image.format == vk::Format::eUndefined) // claim image in this format
                    image.format = vk::Format::eR8G8B8A8Unorm;
                Logger::check(image.format == vk::Format::eR8G8B8A8Unorm, "Format of orm texture must be R8G8B8A8_UNORM");
            } else if (o_image || rm_image) {
                if (o_image && rm_image) {
                    Logger::check(
                            rm_image->width == o_image->width && rm_image->height == o_image->height,
                            "Occlusion texture size doesn't match roughness-metalness texture size"
                    );
                }

                if (orm_cache_map.contains(orm_cache_key)) {
                    mat.ormTexture = orm_cache_map.at(orm_cache_key);
                } else {
                    PlainImageDataU8 *o_or_rm_image = o_image ? o_image : rm_image;
                    mat.ormTexture = static_cast<int32_t>(scene_data.images.size());
                    auto orm_image = PlainImageDataU8::create(
                            vk::Format::eR8G8B8A8Unorm, o_or_rm_image->width, o_or_rm_image->height
                    );

                    if (o_image) {
                        o_image->copyChannels(orm_image, {0});
                    }
                    if (rm_image) {
                        rm_image->copyChannels(orm_image, {1, 2});
                    } else {
                        orm_image.fill({1, 2}, {0xff, 0xff});
                    }
                    orm_cache_map[orm_cache_key] = mat.ormTexture;
                    // pushing vector invalidates pointers, so push last
                    scene_data.images.emplace_back(std::move(orm_image));
                }
            }

            if (gltf_mat.normalTexture.has_value()) {
                auto index = static_cast<int32_t>(
                        asset.textures[gltf_mat.normalTexture.value().textureIndex].imageIndex.value()
                );
                if (normal_cache_map.contains(index)) {
                    mat.normalTexture = normal_cache_map.at(index);
                } else {
                    const auto &src_image = scene_data.images[index];
                    auto normal = PlainImageDataU8::create(vk::Format::eR8G8Unorm, src_image.width, src_image.height);
                    src_image.copyChannels(normal, {0, 1});
                    mat.normalTexture = static_cast<int32_t>(scene_data.images.size());
                    scene_data.images.emplace_back(std::move(normal));
                    normal_cache_map[index] = mat.normalTexture;
                }
            }
        }
    }

    void Loader::loadLight(
            const fastgltf::Asset &asset, Scene &scene_data, const fastgltf::Node &node, const glm::mat4 &transform, Node &scene_node
    ) {
        const auto &light = asset.lights[node.lightIndex.value()];
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
                auto &scene_light = scene_data.spotLights.emplace_back() = {
                    .position = position,
                    .theta = glm::degrees(glm::atan(forward.y, glm::sqrt(forward.x * forward.x + forward.z * forward.z))),
                    .phi = glm::degrees(glm::atan(forward.x, forward.z)),
                    .color = {light.color.x(), light.color.y(), light.color.z()},
                    .power = light.intensity / 683.0f,
                };
                scene_light.outerConeAngle = glm::degrees(light.outerConeAngle.value_or(glm::radians(scene_light.outerConeAngle)));
                scene_light.innerConeAngle = glm::degrees(light.innerConeAngle.value_or(glm::radians(scene_light.innerConeAngle)));
                break;
            }
        }
    }

    void Loader::loadNode(
            const fastgltf::Asset &asset,
            Scene &scene_data,
            const std::vector<PrimitiveInfo> &primitive_infos,
            const std::vector<size_t> &mesh_primitive_table,
            const fastgltf::Node &node,
            const glm::mat4 &transform
    ) {

        size_t node_index = scene_data.nodes.size();
        auto &scene_node = scene_data.nodes.emplace_back() = {
            .name = std::string(node.name),
            .transform = transform,
            .mesh = static_cast<uint32_t>(node.meshIndex.value_or(UINT32_MAX)),
        };

        if (scene_node.mesh == UINT32_MAX) {
            // Non mesh node
            if (node.lightIndex.has_value()) {
                loadLight(asset, scene_data, node, transform, scene_node);
            }
            return;
        }

        const fastgltf::Mesh &mesh = asset.meshes[scene_node.mesh];

        for (size_t i = 0; i < mesh.primitives.size(); i++) {
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

    Loader::Loader() { mParser = std::make_unique<fastgltf::Parser>(fastgltf::Extensions::KHR_lights_punctual); }
    Loader::~Loader() = default;

    Scene Loader::load(const std::filesystem::path &path) const {
        auto data = fastgltf::GltfDataBuffer::FromPath(path);
        if (auto error = data.error(); error != fastgltf::Error::None) {
            Logger::fatal(std::format("Failed to load GLTF: {}", fastgltf::getErrorName(error)));
        }

        auto asset = mParser->loadGltf(data.get(), path.parent_path(), fastgltf::Options::None);
        if (auto error = asset.error(); error != fastgltf::Error::None) {
            Logger::fatal(std::format("Failed to load GLTF: {}", fastgltf::getErrorName(error)));
        }

        Scene scene_data;

        loadImages(asset.get(), scene_data);

        // Since multiple nodes / primitives can share the same mesh data it's required to load it in separate passes
        std::vector<PrimitiveInfo> primitive_infos;
        std::vector<size_t> mesh_primitive_table; // maps mesh index to primitive info start index
        loadMeshData(asset.get(), scene_data, primitive_infos, mesh_primitive_table);
        loadMaterials(asset.get(), scene_data);
        fastgltf::iterateSceneNodes(
                asset.get(), 0, fastgltf::math::fmat4x4(),
                [&](fastgltf::Node &node, fastgltf::math::fmat4x4 matrix) {
                    glm::mat4 transform = glm::make_mat4x4(&matrix.col(0)[0]);
                    loadNode(asset.get(), scene_data, primitive_infos, mesh_primitive_table, node, transform);
                }
        );

        // Sort by material for rendering efficiency
        std::ranges::sort(scene_data.sections, [](const auto &lhs, const auto &rhs) {
            return lhs.material < rhs.material;
        });

        scene_data.index_count = scene_data.index_data.size() / sizeof(uint32_t);
        scene_data.vertex_count = scene_data.vertex_position_data.size() / sizeof(glm::vec3);

        return scene_data;
    }

    Loader::Loader(Loader &&other) noexcept : mParser(std::move(other.mParser)) {}
    Loader &Loader::operator=(Loader &&other) noexcept {
        if (this == &other)
            return *this;
        mParser = std::move(other.mParser);
        return *this;
    }

} // namespace gltf
