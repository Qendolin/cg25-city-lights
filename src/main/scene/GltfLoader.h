#pragma once
#include <fastgltf/core.hpp>
#include <filesystem>
#include <glm/glm.hpp>

#include "../util/Logger.h"

// https://fastgltf.readthedocs.io/v0.9.x/overview.html

struct BoundingBox {
    glm::vec3 min = glm::vec3{std::numeric_limits<float>::infinity()};
    glm::vec3 max = glm::vec3{-std::numeric_limits<float>::infinity()};

    void extend(const glm::vec3 &p) {
        min = glm::min(min, p);
        max = glm::max(max, p);
    }
};

/// <summary>
/// A mesh section which can be rendered using a single draw command.
/// The material is uniform across the section.
/// </summary>
struct Instance {
    uint32_t indexOffset = 0;
    uint32_t indexCount = 0;
    int32_t vertexOffset = 0;
    glm::mat4 transformation = glm::mat4(1.0);
    // index of bounding box
    uint32_t bounds = UINT32_MAX;
    // index of material
    uint32_t material = UINT32_MAX;
};

struct Material {
    // reserved for later use
    int32_t albedoTexture = -1;
    // reserved for later use
    int32_t omrTexture = -1;
    // reserved for later use
    int32_t normalTexture = -1;
    glm::vec4 albedoFactor = glm::vec4(1.0, 1.0, 1.0, 1.0);
    float metalnessFactor = 1.0;
    float roughnessFactor = 1.0;
    float normalFactor = 1.0;
};

struct SceneData {
    size_t index_count = 0;
    size_t vertex_count = 0;

    std::vector<glm::vec3> vertex_position_data;
    std::vector<glm::vec3> vertex_normal_data;
    std::vector<glm::vec4> vertex_tangent_data;
    std::vector<glm::vec2> vertex_texcoord_data;
    std::vector<glm::uint32> index_data;

    std::vector<BoundingBox> bounds;
    std::vector<Instance> instances;
    std::vector<Material> materials;
};

class GltfLoader {
public:
    SceneData load(const std::filesystem::path &path);

private:
    struct PrimitiveInfo {
        uint32_t indexOffset;
        uint32_t indexCount;
        int32_t vertexOffset;
    };

    static void loadMeshData(
            const fastgltf::Asset &asset,
            SceneData &scene_data,
            std::vector<PrimitiveInfo> &primitive_infos,
            std::vector<size_t> &mesh_primitive_table
    );

    static void loadMaterials(const fastgltf::Asset &asset, SceneData &scene_data);

    static void loadNode(
            const fastgltf::Asset &asset,
            SceneData &scene_data,
            const std::vector<PrimitiveInfo> &primitive_infos,
            const std::vector<size_t> &mesh_primitive_table,
            fastgltf::Node &node,
            fastgltf::math::fmat4x4 matrix
    );

    fastgltf::Parser parser;
};
