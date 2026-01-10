#pragma once

#include <glm/gtc/quaternion.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>
#include <map>
#include <vector>

#include "../util/math.h"


struct DirectionalLight;
struct SpotLight;
struct PointLight;
template<typename T>
class PlainImageData;

namespace gltf {

    struct Mesh {
        /// <summary>
        /// The unique name of this mesh.
        /// </summary>
        std::string name = "";

        /// <summary>
        /// The bounding box for this mesh.
        /// </summary>
        util::BoundingBox bounds;
    };

    struct Node {
        /// <summary>
        /// The unique name of this node.
        /// </summary>
        std::string name = "";
        /// <summary>
        /// The transformation matrix for this node.
        /// </summary>
        glm::mat4 transform = glm::mat4(1.0);
        /// <summary>
        /// The index of the mesh for this node or UINT32_MAX if none.
        /// </summary>
        uint32_t mesh = UINT32_MAX;
        /// <summary>
        /// The index of the point light for this node or UINT32_MAX if none.
        /// </summary>
        uint32_t pointLight = UINT32_MAX;
        /// <summary>
        /// The index of the spot light for this node or UINT32_MAX if none.
        /// </summary>
        uint32_t spotLight = UINT32_MAX;
        /// <summary>
        /// The index of the directional light for this node or UINT32_MAX if none.
        /// </summary>
        uint32_t directionalLight = UINT32_MAX;
        /// <summary>
        /// The index of the imported animation associated with this node or UINT32_MAX if none.
        /// </summary>
        uint32_t animation = UINT32_MAX;
        /// <summary>
        /// Flag that indicates if the node is a camera or not.
        /// </summary>
        bool isAnimatedCamera = false;
    };

    /// <summary>
    /// A mesh section which can be rendered using a single draw command.
    /// The material is uniform across the section.
    /// </summary>
    struct Section {
        /// <summary>
        /// The offset into the global index buffer.
        /// </summary>
        uint32_t indexOffset = 0;
        /// <summary>
        /// The number of indices in this section.
        /// </summary>
        uint32_t indexCount = 0;
        /// <summary>
        /// The offset into the global vertex buffer.
        /// </summary>
        int32_t vertexOffset = 0;
        /// <summary>
        /// The index for the node of this section.
        /// </summary>
        uint32_t node = UINT32_MAX;
        /// <summary>
        /// The index of the bounding box for this section.
        /// </summary>
        uint32_t bounds = UINT32_MAX;
        /// <summary>
        /// The index of the material for this section.
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
        /// The index of the occlusion-roughness-metallic (ORM) texture, or -1 if none.
        /// </summary>
        int32_t ormTexture = -1;
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

    struct Animation {
        std::vector<float> translation_timestamps;
        std::vector<float> rotation_timestamps;
        std::vector<float> scale_timestamps;
        std::vector<glm::vec3> translations;
        std::vector<glm::vec4> rotations;
        std::vector<glm::vec3> scales;
    };

    /// <summary>
    /// Holds all the data for a loaded scene.
    /// </summary>
    struct Scene {

        Scene();
        ~Scene();

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
        /// A list of bounding boxes for the sections in the scene. They are in local space.
        /// </summary>
        std::vector<util::BoundingBox> bounds;
        /// <summary>
        /// A list of all mesh sections in the scene.
        /// </summary>
        std::vector<Section> sections;
        /// <summary>
        /// A list of all materials in the scene.
        /// </summary>
        std::vector<Material> materials;
        /// <summary>
        /// A list of all nodes in the scene.
        /// </summary>
        std::vector<Node> nodes;
        /// <summary>
        /// A list of all meshes in the scene.
        /// </summary>
        std::vector<Mesh> meshes;
        /// <summary>
        /// A list of all images in the scene.
        /// </summary>
        std::vector<PlainImageData<uint8_t>> images;
        /// <summary>
        /// A list of all point lights in the scene.
        /// </summary>
        std::vector<PointLight> pointLights;
        /// <summary>
        /// A list of all spot lights in the scene.
        /// </summary>
        std::vector<SpotLight> spotLights;
        /// <summary>
        /// A list of all directional lights in the scene.
        /// </summary>
        std::vector<DirectionalLight> directionalLights;
        /// <summary>
        /// A list of all node animations in the scene. Only translations and rotations components
        /// are currently stored by the animation struct.
        /// </summary>
        std::vector<Animation> animations;
    };
} // namespace gltf
