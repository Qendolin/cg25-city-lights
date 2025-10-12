#include <fastgltf/core.hpp>
#include <fastgltf/glm_element_traits.hpp>
#include <fastgltf/math.hpp>
#include <glm/gtc/type_ptr.hpp>.

#include "../util/Logger.h"
#include "Gltf.h"

template<typename T>
static void appendFromAccessor(std::vector<T> &dest, const fastgltf::Asset &asset, const fastgltf::Accessor &accessor) {
    size_t old_size = dest.size();
    dest.resize(old_size + accessor.count);
    fastgltf::copyFromAccessor<T>(asset, accessor, dest.data() + old_size);
}

namespace gltf {

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

            auto& scene_mesh = scene_data.meshes.emplace_back() = {
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

                BoundingBox &bounds = scene_data.bounds.emplace_back() = {};
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
                index_offset += index_accessor.count;
                vertex_offset += position_accessor.count;
            }
        }
    }
    void Loader::loadMaterials(const fastgltf::Asset &asset, Scene &scene_data) {
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
        const auto& scene_node = scene_data.nodes.emplace_back() = {
            .name = std::string(node.name),
            .transform = transform,
            .mesh = static_cast<uint32_t>(node.meshIndex.value_or(UINT32_MAX)),
        };

        if (scene_node.mesh == UINT32_MAX)
            return;

        const fastgltf::Mesh &mesh = asset.meshes[scene_node.mesh];

        for (size_t i = 0; i < mesh.primitives.size(); i++) {
            const PrimitiveInfo &primitive_info = primitive_infos[mesh_primitive_table[scene_node.mesh] + i];

            scene_data.sections.emplace_back() = {
                .indexOffset = primitive_info.indexOffset,
                .indexCount = primitive_info.indexCount,
                .vertexOffset = primitive_info.vertexOffset,
                .node = static_cast<uint32_t>(node_index),
                .bounds = static_cast<uint32_t>(scene_data.bounds.size()),
                .material = primitive_info.material,
            };
        }
    }

    Loader::Loader() { mParser = std::make_unique<fastgltf::Parser>(); }
    Loader::~Loader() {}

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
        std::vector<PrimitiveInfo> primitive_infos;
        std::vector<size_t> mesh_primitive_table; // maps mesh index to primitive info start index

        // Since multiple nodes / primitives can share the same mesh data it's required to load it in seperate passes
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
