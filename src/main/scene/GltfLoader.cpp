#include "GltfLoader.h"

#include <fastgltf/glm_element_traits.hpp>
#include <glm/gtc/type_ptr.hpp>.

template<typename T>
void appendFromAccessor(std::vector<T> &dest, const fastgltf::Asset &asset, const fastgltf::Accessor &accessor) {
    size_t old_size = dest.size();
    dest.resize(old_size + accessor.count);
    fastgltf::copyFromAccessor<T>(asset, accessor, dest.data() + old_size);
}

void GltfLoader::loadMeshData(
        const fastgltf::Asset &asset,
        SceneData &scene_data,
        std::vector<PrimitiveInfo> &primitive_infos,
        std::vector<size_t> &mesh_primitive_table
) {
    uint32_t vertex_offset = 0;
    uint32_t index_offset = 0;
    for (const auto &mesh: asset.meshes) {
        mesh_primitive_table.emplace_back(primitive_infos.size());
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
                fastgltf::iterateAccessorWithIndex<std::uint32_t>(asset, index_accessor, [&](std::uint16_t index, size_t i) {
                    scene_data.index_data[old_size + i] = index;
                });
            } else {
                Logger::check(false, std::format("Mesh '{}' has indices which aren't unsigned shorts or unsigned ints", mesh.name));
            }
            scene_data.index_count += index_accessor.count;

            primitive_infos.emplace_back() = {
                .indexOffset = index_offset,
                .indexCount = static_cast<uint32_t>(index_accessor.count),
                .vertexOffset = static_cast<int32_t>(vertex_offset),
            };
            index_offset += index_accessor.count;
            vertex_offset += position_accessor.count;
        }
    }
}
void GltfLoader::loadMaterials(const fastgltf::Asset &asset, SceneData &scene_data) {
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
void GltfLoader::loadNode(
        const fastgltf::Asset &asset,
        SceneData &scene_data,
        const std::vector<PrimitiveInfo> &primitive_infos,
        const std::vector<size_t> &mesh_primitive_table,
        fastgltf::Node &node,
        fastgltf::math::fmat4x4 matrix
) {
    if (!node.meshIndex.has_value())
        return;

    size_t mesh_index = node.meshIndex.value();
    const fastgltf::Mesh &mesh = asset.meshes[mesh_index];
    glm::mat4 transform = glm::make_mat4x4(&matrix.col(0)[0]);

    for (size_t i = 0; i < mesh.primitives.size(); i++) {
        const PrimitiveInfo &primitive_info = primitive_infos[mesh_primitive_table[mesh_index] + i];

        const fastgltf::Accessor &position_accessor =
                asset.accessors[mesh.primitives[i].findAttribute("POSITION")->accessorIndex];

        scene_data.instances.emplace_back() = {
            .indexOffset = primitive_info.indexOffset,
            .indexCount = primitive_info.indexCount,
            .vertexOffset = primitive_info.vertexOffset,
            .transformation = transform,
            .bounds = static_cast<uint32_t>(scene_data.bounds.size()),
        };

        BoundingBox &bounds = scene_data.bounds.emplace_back() = {};
        for (const glm::vec3 &p: fastgltf::iterateAccessor<glm::vec3>(asset, position_accessor)) {
            bounds.extend(transform * glm::vec4(p, 1.0));
        }
    }
}

SceneData GltfLoader::load(const std::filesystem::path &path) {
    auto data = fastgltf::GltfDataBuffer::FromPath(path);
    if (auto error = data.error(); error != fastgltf::Error::None) {
        Logger::fatal(std::format("Failed to load GLTF: {}", fastgltf::getErrorName(error)));
    }

    auto asset = parser.loadGltf(data.get(), path.parent_path(), fastgltf::Options::None);
    if (auto error = asset.error(); error != fastgltf::Error::None) {
        Logger::fatal(std::format("Failed to load GLTF: {}", fastgltf::getErrorName(error)));
    }

    SceneData scene_data;
    std::vector<PrimitiveInfo> primitive_infos;
    std::vector<size_t> mesh_primitive_table; // maps mesh index to primitive info start index

    // Since multiple nodes / primitives can share the same mesh data it's required to load it in seperate passes
    loadMeshData(asset.get(), scene_data, primitive_infos, mesh_primitive_table);
    loadMaterials(asset.get(), scene_data);
    fastgltf::iterateSceneNodes(
            asset.get(), 0, fastgltf::math::fmat4x4(),
            [&](fastgltf::Node &node, fastgltf::math::fmat4x4 matrix) {
                loadNode(asset.get(), scene_data, primitive_infos, mesh_primitive_table, node, matrix);
            }
    );

    // Sort by material for rendering efficiency
    std::ranges::sort(scene_data.instances, [](const auto &lhs, const auto &rhs) { return lhs.material < rhs.material; });

    scene_data.index_count = scene_data.index_data.size() / sizeof(uint32_t);
    scene_data.vertex_count = scene_data.vertex_position_data.size() / sizeof(glm::vec3);

    return scene_data;
}
