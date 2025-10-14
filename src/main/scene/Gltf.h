#pragma once
#include <filesystem>
#include <glm/glm.hpp>

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
    public:
        Loader();
        ~Loader();

        /// <summary>
        /// Loads a glTF scene from the given file path.
        /// </summary>
        /// <param name="path">The path to the glTF file.</param>
        /// <returns>The loaded scene data.</returns>
        [[nodiscard]] Scene load(const std::filesystem::path &path) const;

        Loader(const Loader &other) = delete;
        Loader(Loader &&other) noexcept;
        Loader &operator=(const Loader &other) = delete;
        Loader &operator=(Loader &&other) noexcept;

    private:
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

        /// <summary>
        /// Recursively loads a node and its children from the glTF asset.
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

        std::unique_ptr<fastgltf::Parser> mParser;
    };

} // namespace gltf
