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

    Loader::Loader(ImageCpuLoader *imageLoader) : mImageLoader(imageLoader) {
        mParser = std::make_unique<fastgltf::Parser>(fastgltf::Extensions::KHR_lights_punctual);
    }
    Loader::~Loader() = default;

    Loader::Loader(Loader &&other) noexcept
        : mParser(std::move(other.mParser)), mImageLoader(std::move(other.mImageLoader)) {}
    Loader &Loader::operator=(Loader &&other) noexcept {
        if (this == &other)
            return *this;
        mParser = std::move(other.mParser);
        mImageLoader = std::move(other.mImageLoader);
        return *this;
    }

    Scene Loader::load(const std::filesystem::path &path) const {
        fastgltf::Asset asset = assetFromPath(path);
        Scene scene_data;
        // Since multiple nodes/primitives can share same mesh data it's required to load in separate passes
        std::vector<PrimitiveInfo> primitive_infos;
        std::vector<size_t> mesh_primitive_table; // Maps mesh index to primitive info start index

        loadImages(asset, scene_data, *mImageLoader);
        loadMeshData(asset, scene_data, primitive_infos, mesh_primitive_table);
        loadMaterials(asset, scene_data);
        loadNodes(asset, primitive_infos, mesh_primitive_table, scene_data);

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
            std::vector<std::size_t> mesh_primitive_table,
            Scene &scene_data
    ) {
        fastgltf::iterateSceneNodes(asset, 0, fastgltf::math::fmat4x4(), [&](fastgltf::Node &node, fastgltf::math::fmat4x4 matrix) {
            glm::mat4 transform = glm::make_mat4x4(&matrix.col(0)[0]);
            loadNode(asset, scene_data, primitive_infos, mesh_primitive_table, node, transform);
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

                index_offset += primitive_counts.index_count;
                vertex_offset += primitive_counts.vertex_count;
            }
        }
    }

    void Loader::loadImages(const fastgltf::Asset &asset, Scene &scene_data, ImageCpuLoader &image_loader) {
        fastgltf::DefaultBufferDataAdapter adapter = {};
        for (const auto &image: asset.images) {
            Logger::check(
                    std::holds_alternative<fastgltf::sources::BufferView>(image.data),
                    "Image data source must be a buffer view"
            );
            size_t buffer_view_index = std::get<fastgltf::sources::BufferView>(image.data).bufferViewIndex;
            auto src_data = adapter(asset, buffer_view_index);

            auto source = ImageSource(src_data);
            auto task = image_loader.loadAsync(source);
            scene_data.images.emplace_back() = {.info = source.info, .type = ImageType::Undefined, .task = task};
        }
    }

    int32_t Loader::combineOrmImages(
            const fastgltf::Asset &asset,
            Scene &scene_data,
            std::map<std::pair<int32_t, int32_t>, int32_t> &cache,
            const fastgltf::Material &gltf_mat
    ) {
        int32_t mr_index = getTextureImageIndex(asset, gltf_mat.pbrData.metallicRoughnessTexture);
        int32_t o_index = getTextureImageIndex(asset, gltf_mat.occlusionTexture);

        if (mr_index < 0 && o_index < 0) {
            return -1;
        }

        std::pair<int32_t, int32_t> cache_key = std::make_pair(mr_index, o_index);
        if (cache.contains(cache_key)) {
            return cache[cache_key];
        }

        auto &mr_future = scene_data.images[mr_index];
        auto &o_future = scene_data.images[o_index];

        if (mr_future.info.width != o_future.info.width || mr_future.info.height != o_future.info.height) {
            throw std::runtime_error("Image has different size");
        }

        if (mr_future.info.componentType.size != o_future.info.componentType.size) {
            throw std::runtime_error("Image has component size");
        }

        auto aggregate = LoadTask<ImageData>::when_all({o_future.task, mr_future.task});
        auto merged = aggregate.then([](const std::vector<const ImageData *> &all) {
            auto &o = *all[0];
            auto &mr = *all[1];
            auto omr = ImageData::create(o, ComponentType::UInt8, 4);
            ImageData::copy(o, omr, 0, 0);
            ImageData::copy(mr, omr, 1, 1);
            ImageData::copy(mr, omr, 2, 2);
            return omr;
        });
        ImageSourceInfo info = mr_future.info;
        info.channels = 4;
        scene_data.images.emplace_back() = {.info = info, .type = ImageType::OcclusionMetalnessRoughness, .task = merged};
        return static_cast<int32_t>(scene_data.images.size() - 1);
    }

    static void assignImageType(Scene &scene_data, int32_t image_index, ImageType image_type) {
        if (image_index < 0)
            return;
        auto &image = scene_data.images[image_index];
        if (image.type != ImageType::Undefined && image.type != image_type)
            throw std::runtime_error("Image type inconsistent");
        image.type = ImageType::Albedo;
    }

    void Loader::loadMaterials(const fastgltf::Asset &asset, Scene &scene_data) {
        // Occlusion and roughness-metalness images may need to be merged. This cache is used for deduplication.
        std::map<std::pair<int32_t, int32_t>, int32_t> orm_cache_map;
        std::map<int32_t, int32_t> normal_cache_map;
        std::map<int32_t, int32_t> albedo_cache_map;

        for (const auto &gltf_mat: asset.materials) {
            Material mat = initMaterialWithFactors(gltf_mat);

            int32_t albedo_index = getTextureImageIndex(asset, gltf_mat.pbrData.baseColorTexture);
            if (albedo_cache_map.contains(albedo_index)) {
                mat.albedoTexture = albedo_cache_map[albedo_index];
            } else if (albedo_index >= 0) {
                auto &future = scene_data.images[albedo_index];
                auto task = future.task.then([](const ImageData &image_data) {
                    auto rgba = ImageData::create(image_data, ComponentType::UInt8, 4);
                    ImageData::copy(image_data, rgba, 0, 0);
                    ImageData::copy(image_data, rgba, 1, 1);
                    ImageData::copy(image_data, rgba, 2, 2);
                    if (image_data.components == 4) {
                        ImageData::copy(image_data, rgba, 3, 3);
                    } else {
                        ImageData::fill(rgba, 3, ComponentType::UInt8.one.data());
                    }
                    return rgba;
                });
                mat.albedoTexture = scene_data.images.size();
                albedo_cache_map[albedo_index] = mat.albedoTexture;
                ImageSourceInfo info = future.info;
                info.channels = 4;
                scene_data.images.emplace_back() = {.info = info, .type = ImageType::Albedo, .task = task};
            }

            mat.ormTexture = combineOrmImages(asset, scene_data, orm_cache_map, gltf_mat);

            int32_t normal_index = getTextureImageIndex(asset, gltf_mat.normalTexture);
            if (normal_cache_map.contains(normal_index)) {
                mat.normalTexture = normal_cache_map[normal_index];
            } else if (normal_index >= 0) {
                auto &future = scene_data.images[normal_index];
                auto task = future.task.then([](const ImageData &image_data) {
                    auto rg = ImageData::create(image_data, ComponentType::UInt8, 2);
                    ImageData::copy(image_data, rg, 0, 0);
                    ImageData::copy(image_data, rg, 1, 1);
                    return rg;
                });
                mat.normalTexture = scene_data.images.size();
                albedo_cache_map[albedo_index] = mat.normalTexture;
                ImageSourceInfo info = future.info;
                info.channels = 2;
                scene_data.images.emplace_back() = {.info = info, .type = ImageType::Normal, .task = task};
            }

            // assignImageType(scene_data, mat.albedoTexture, ImageType::Albedo);
            // assignImageType(scene_data, mat.ormTexture, ImageType::OcclusionMetalnessRoughness);
            // assignImageType(scene_data, mat.normalTexture, ImageType::Normal);

            scene_data.materials.push_back(mat);
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

    void Loader::loadLight(
            const fastgltf::Asset &asset, Scene &scene_data, const fastgltf::Node &node, const glm::mat4 &transform, Node &scene_node
    ) {
        const auto &light = asset.lights[node.lightIndex.value()];
        glm::vec3 position = transform[3];
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

    fastgltf::Asset Loader::assetFromPath(const std::filesystem::path &path) const {
        auto data = fastgltf::GltfDataBuffer::FromPath(path);

        if (auto error = data.error(); error != fastgltf::Error::None)
            Logger::fatal(std::format("Failed to load GLTF: {}", fastgltf::getErrorName(error)));

        auto asset = mParser->loadGltf(data.get(), path.parent_path(), fastgltf::Options::None);

        if (auto error = asset.error(); error != fastgltf::Error::None)
            Logger::fatal(std::format("Failed to load GLTF: {}", fastgltf::getErrorName(error)));

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
        return static_cast<uint32_t>(index_accessor.count);
    }

} // namespace gltf
