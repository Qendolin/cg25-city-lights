#pragma once
#include <fastgltf/core.hpp>
#include <filesystem>
#include <glm/glm.hpp>
#include <map>
#include <string_view>

#include "gltf_types.h"

// https://fastgltf.readthedocs.io/v0.9.x/overview.html

namespace fastgltf {
    class Parser;
    struct Node;
    class Asset;
}

namespace gltf {

    /// <summary>
    /// A loader for glTF 2.0 files.
    /// </summary>
    class Loader {
    private:
        std::unique_ptr<fastgltf::Parser> mParser;

    public:
        Loader();
        ~Loader();

        Loader(const Loader &other) = delete;
        Loader(Loader &&other) noexcept;
        Loader &operator=(const Loader &other) = delete;
        Loader &operator=(Loader &&other) noexcept;

        /// <summary>
        /// Loads a glTF scene from the given file path.
        /// </summary>
        /// <param name="path">The path to the glTF file.</param>
        /// <returns>The loaded scene data.</returns>
        [[nodiscard]] Scene load(const std::filesystem::path &path) const;

    private:
        struct PrimitiveCounts {
            uint32_t index_count;
            uint32_t vertex_count;
        };

        /// <summary>
        /// Information about a single primitive (a part of a mesh).
        /// </summary>
        struct PrimitiveInfo {
            /// <summary>
            /// The offset into the global index buffer.
            /// </summary>
            uint32_t indexOffset = 0;
            /// <summary>
            /// The number of indices in this primitive.
            /// </summary>
            uint32_t indexCount = 0;
            /// <summary>
            /// The offset into the global vertex buffer.
            /// </summary>
            int32_t vertexOffset = 0;
            /// <summary>
            /// The index of the material for this primitive.
            /// </summary>
            uint32_t material = UINT32_MAX;
            /// <summary>
            /// The index of the bounding box for this primitive.
            /// </summary>
            uint32_t bounds = UINT32_MAX;
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
                Scene &scene_data,
                std::vector<PrimitiveInfo> &primitive_infos,
                std::vector<size_t> &mesh_primitive_table
        );

        /// <summary>
        /// Loads all images from the glTF asset.
        /// </summary>
        /// <param name="asset">The glTF asset.</param>
        /// <param name="scene_data">The scene data to populate.</param>
        static void loadImages(const fastgltf::Asset &asset, Scene &scene_data);

        /// <summary>
        /// Loads all materials from the glTF asset.
        /// </summary>
        /// <param name="asset">The glTF asset.</param>
        /// <param name="scene_data">The scene data to populate.</param>
        static void loadMaterials(const fastgltf::Asset &asset, Scene &scene_data);

        // TODO: Summary
        static void loadNodes(
                fastgltf::Asset &asset,
                const std::vector<PrimitiveInfo> &primitive_infos,
                std::vector<std::size_t> mesh_primitive_table,
                Scene &scene_data
        );

        /// <summary>
        /// Loads a single light.
        /// </summary>
        /// <param name="asset">The glTF asset.</param>
        /// <param name="scene_data">The scene data to populate.</param>
        /// <param name="node">The node containing the light.</param>
        /// <param name="transform">The transformation matrix of the node.</param>
        /// <param name="scene_node">The scene node to populate.</param>
        static void loadLight(
                const fastgltf::Asset &asset, Scene &scene_data, const fastgltf::Node &node, const glm::mat4 &transform, Node &scene_node
        );

        /// <summary>
        /// Loads a single node.
        /// </summary>
        /// <param name="asset">The glTF asset.</param>
        /// <param name="scene_data">The scene data to populate.</param>
        /// <param name="primitive_infos">A list of primitive information.</param>
        /// <param name="mesh_primitive_table">A table mapping mesh index to the start of its primitives in primitive_infos.</param>
        /// <param name="node">The node to load.</param>
        /// <param name="transform">The transformation matrix of the node.</param>
        static void loadNode(
                const fastgltf::Asset &asset,
                Scene &scene_data,
                const std::vector<PrimitiveInfo> &primitive_infos,
                const std::vector<size_t> &mesh_primitive_table,
                const fastgltf::Node &node,
                const glm::mat4 &transform
        );

        static PrimitiveCounts loadPrimitive(
                const fastgltf::Asset &asset,
                const fastgltf::Primitive &primitive,
                std::string_view mesh_name,
                Scene &scene_data,
                gltf::Mesh &scene_mesh
        );

        static Material initMaterialWithFactors(const fastgltf::Material &gltf_mat);

        static int32_t loadMaterialAlbedoTexture(
                const fastgltf::Asset &asset, const fastgltf::Material &gltf_mat, Scene &scene_data
        );

        static int32_t loadMaterialOrmTexture(
                const fastgltf::Asset &asset,
                const fastgltf::Material &gltf_mat,
                Scene &scene_data,
                std::map<std::pair<int32_t, int32_t>, int32_t> &orm_cache_map
        );

        static int32_t loadMaterialNormalTexture(
                const fastgltf::Asset &asset,
                const fastgltf::Material &gltf_mat,
                Scene &scene_data,
                std::map<int32_t, int32_t> &normal_cache_map
        );
        
        fastgltf::Asset assetFromPath(const std::filesystem::path &path) const;

        static const fastgltf::Accessor &getAttributeAccessor(
                const fastgltf::Asset &asset,
                const fastgltf::Primitive &primitive,
                const std::string_view attribute_name,
                const std::string_view mesh_name
        );

        static uint32_t appendMeshPrimitiveIndices(
                const fastgltf::Asset &asset, const fastgltf::Primitive &primitive, std::string_view mesh_name, gltf::Scene &scene_data
        );
    };

} // namespace gltf
