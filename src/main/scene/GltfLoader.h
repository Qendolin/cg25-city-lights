#pragma once
#include <fastgltf/core.hpp>
#include <filesystem>
#include <glm/glm.hpp>

#include "../util/Logger.h"

// https://fastgltf.readthedocs.io/v0.9.x/overview.html

/// <summary>
/// Represents an axis-aligned bounding box.
/// </summary>
struct BoundingBox {
    /// <summary>
    /// The minimum corner of the bounding box.
    /// </summary>
    glm::vec3 min = glm::vec3{std::numeric_limits<float>::infinity()};
    /// <summary>
    /// The maximum corner of the bounding box.
    /// </summary>
    glm::vec3 max = glm::vec3{-std::numeric_limits<float>::infinity()};

    /// <summary>
    /// Extends the bounding box to include the given point.
    /// </summary>
    /// <param name="p">The point to include.</param>
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
    /// <summary>
    /// The offset into the global index buffer.
    /// </summary>
    uint32_t indexOffset = 0;
    /// <summary>
    /// The number of indices in this instance.
    /// </summary>
    uint32_t indexCount = 0;
    /// <summary>
    /// The offset into the global vertex buffer.
    /// </summary>
    int32_t vertexOffset = 0;
    /// <summary>
    /// The transformation matrix for this instance.
    /// </summary>
    glm::mat4 transform = glm::mat4(1.0);
    /// <summary>
    /// The index of the bounding box for this instance.
    /// </summary>
    uint32_t bounds = UINT32_MAX;
    /// <summary>
    /// The index of the material for this instance.
    /// </summary>
    uint32_t material = UINT32_MAX;
};

/// <summary>
/// Represents a PBR material.
/// </summary>
struct Material {
    /// <summary>
    /// The index of the albedo texture, or -1 if none.
    /// </summary>
    int32_t albedoTexture = -1;
    /// <summary>
    /// The index of the occlusion-metallic-roughness (OMR) texture, or -1 if none.
    /// </summary>
    int32_t omrTexture = -1;
    /// <summary>
    /// The index of the normal texture, or -1 if none.
    /// </summary>
    int32_t normalTexture = -1;
    /// <summary>
    /// The albedo color factor.
    /// </summary>
    glm::vec4 albedoFactor = glm::vec4(1.0, 1.0, 1.0, 1.0);
    /// <summary>
    /// The metalness factor.
    /// </summary>
    float metalnessFactor = 1.0;
    /// <summary>
    /// The roughness factor.
    /// </summary>
    float roughnessFactor = 1.0;
    /// <summary>
    /// The normal map scale factor.
    /// </summary>
    float normalFactor = 1.0;
};

/// <summary>
/// Holds all the data for a loaded scene.
/// </summary>
struct SceneData {
    /// <summary>
    /// The total number of indices in the scene.
    /// </summary>
    size_t index_count = 0;
    /// <summary>
    /// The total number of vertices in the scene.
    /// </summary>
    size_t vertex_count = 0;

    /// <summary>
    /// Vertex position data.
    /// </summary>
    std::vector<glm::vec3> vertex_position_data;
    /// <summary>
    /// Vertex normal data.
    /// </summary>
    std::vector<glm::vec3> vertex_normal_data;
    /// <summary>
    /// Vertex tangent data.
    /// </summary>
    std::vector<glm::vec4> vertex_tangent_data;
    /// <summary>
    /// Vertex texture coordinate data.
    /// </summary>
    std::vector<glm::vec2> vertex_texcoord_data;
    /// <summary>
    /// Index data.
    /// </summary>
    std::vector<glm::uint32> index_data;

    /// <summary>
    /// A list of bounding boxes for the instances in the scene.
    /// </summary>
    std::vector<BoundingBox> bounds;
    /// <summary>
    /// A list of all instances in the scene.
    /// </summary>
    std::vector<Instance> instances;
    /// <summary>
    /// A list of all materials in the scene.
    /// </summary>
    std::vector<Material> materials;
};

/// <summary>
/// A loader for glTF 2.0 files.
/// </summary>
class GltfLoader {
public:
    /// <summary>
    /// Loads a glTF scene from the given file path.
    /// </summary>
    /// <param name="path">The path to the glTF file.</param>
    /// <returns>The loaded scene data.</returns>
    SceneData load(const std::filesystem::path &path);

private:
    /// <summary>
    /// Information about a single primitive (a part of a mesh).
    /// </summary>
    struct PrimitiveInfo {
        /// <summary>
        /// The offset into the global index buffer.
        /// </summary>
        uint32_t indexOffset;
        /// <summary>
        /// The number of indices in this primitive.
        /// </summary>
        uint32_t indexCount;
        /// <summary>
        /// The offset into the global vertex buffer.
        /// </summary>
        int32_t vertexOffset;
        /// <summary>
        /// The index of the material for this primitive.
        /// </summary>
        uint32_t material = UINT32_MAX;
    };

    /// <summary>
    /// Loads all mesh data from the glTF asset.
    /// </summary>
    /// <param name="asset">The glTF asset.</param>
    /// <param name="scene_data">The scene data to populate.</param>
    /// <param name="primitive_infos">A list to be populated with primitive information.</param>
    /// <param name="mesh_primitive_table">A table mapping mesh index to the start of its primitives in primitive_infos.</param>
    static void loadMeshData(
            const fastgltf::Asset &asset,
            SceneData &scene_data,
            std::vector<PrimitiveInfo> &primitive_infos,
            std::vector<size_t> &mesh_primitive_table
    );

    /// <summary>
    /// Loads all materials from the glTF asset.
    /// </summary>
    /// <param name="asset">The glTF asset.</param>
    /// <param name="scene_data">The scene data to populate.</param>
    static void loadMaterials(const fastgltf::Asset &asset, SceneData &scene_data);

    /// <summary>
    /// Recursively loads a node and its children from the glTF asset.
    /// </summary>
    /// <param name="asset">The glTF asset.</param>
    /// <param name="scene_data">The scene data to populate.</param>
    /// <param name="primitive_infos">A list of primitive information.</param>
    /// <param name="mesh_primitive_table">A table mapping mesh index to the start of its primitives in primitive_infos.</param>
    /// <param name="node">The node to load.</param>
    /// <param name="matrix">The transformation matrix of the parent node.</param>
    static void loadNode(
            const fastgltf::Asset &asset,
            SceneData &scene_data,
            const std::vector<PrimitiveInfo> &primitive_infos,
            const std::vector<size_t> &mesh_primitive_table,
            fastgltf::Node &node,
            fastgltf::math::fmat4x4 matrix
    );

    /// <summary>
    /// The glTF parser.
    /// </summary>
    fastgltf::Parser parser;
};
